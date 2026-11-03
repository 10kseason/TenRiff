// The two bundled silent loops share one menu-owned decoder and GPU bitmap.
// Their clock never uses chart/audio time or a gameplay decoder.
void MenuWindow::clear_menu_motion_background() {
    if (!d2d_) return;
    if (d2d_->menu_motion_decoder) {
        d2d_->menu_motion_decoder->clear();
        d2d_->menu_motion_decoder.reset();
    }
    d2d_->menu_motion_bitmap.Reset();
    auto& stats = d2d_->menu_motion_stats;
    stats.active = false;
    stats.decoder_active = false;
    stats.has_frame = false;
    stats.reduced_motion = false;
    stats.source = MenuMotionSource::None;
    stats.duration_ns = 0;
    stats.frame_rate = 0;
    stats.asset_available = false;
    stats.failed = false;
    stats.position_ns = 0;
    stats.frame_timestamp_100ns = -1;
    d2d_->menu_motion_clock_ns = 0;
    d2d_->menu_motion_first_request_ns = 0;
    d2d_->menu_motion_last_ready_ns = 0;
    d2d_->menu_motion_request_tick = -1;
    d2d_->menu_motion_path.clear();
    // Per-asset failure latches survive a visit. A broken Result video neither
    // retries every visit nor disables the independent Song Select asset.
}

MenuWindow::MenuMotionDiagnostics MenuWindow::menu_motion_background_diagnostics() const {
    return d2d_ ? d2d_->menu_motion_stats : MenuMotionDiagnostics{};
}

void MenuWindow::update_menu_motion_background(MenuMotionSource source, bool reduced_motion,
                                              int64_t now_ns) {
    if (!d2d_) return;
    if (source == MenuMotionSource::None) {
        clear_menu_motion_background();
        return;
    }
    auto& stats = d2d_->menu_motion_stats;
    const auto asset_index = static_cast<std::size_t>(source);
    if (stats.source != source) {
        clear_menu_motion_background();
        stats.source = source;
        const bool result = source == MenuMotionSource::Result;
        stats.duration_ns = result ? 60'000'000'000LL : 12'000'000'000LL;
        stats.frame_rate = result ? 60 : 30;
        const auto title_path = built_in_title_background_path();
        if (!title_path.empty()) {
            const auto path = util::path_from_utf8_lossy(title_path).parent_path() /
                (result ? "continue_result_bga_ko_1080p60.mp4" : "song-select-loop.mp4");
            std::error_code ec;
            stats.asset_available = std::filesystem::is_regular_file(path, ec) && !ec;
            if (stats.asset_available) d2d_->menu_motion_path = path.u8string();
        }
        if (!stats.asset_available) d2d_->menu_motion_failed_assets[asset_index] = true;
        stats.failed = d2d_->menu_motion_failed_assets[asset_index];
    }
    const bool was_reduced_motion = stats.reduced_motion;
    stats.active = true;
    stats.reduced_motion = reduced_motion;
    if (!d2d_->d2d_context || stats.failed) return;
    const int64_t loop_ns = stats.duration_ns;
    constexpr int64_t decode_timeout_ns = 5'000'000'000LL;
    if (stats.has_frame && (was_reduced_motion || now_ns - d2d_->menu_motion_clock_ns >= decode_timeout_ns)) {
        // Resuming a frozen/minimized menu is not evidence of decoder failure.
        d2d_->menu_motion_last_ready_ns = now_ns;
        d2d_->menu_motion_clock_ns = now_ns;
    }
    if (!d2d_->menu_motion_decoder) {
        d2d_->menu_motion_decoder = std::make_unique<BgaVideoDecoder>();
        stats.decoder_active = true;
        d2d_->menu_motion_clock_ns = now_ns;
        d2d_->menu_motion_first_request_ns = now_ns;
        // Do not send changing timestamps until the first frame succeeds. A
        // failed open receives one request, rather than a per-frame retry.
        d2d_->menu_motion_decoder->request(d2d_->menu_motion_path, 0.0);
        ++stats.decode_requests;
        d2d_->menu_motion_request_tick = 0;
    }
    const bool previously_ready = stats.has_frame;
    if (const auto frame = d2d_->menu_motion_decoder->take_ready();
        frame && frame->source_path == d2d_->menu_motion_path && frame->width > 0 && frame->height > 0 &&
        frame->bgra.size() == static_cast<std::size_t>(frame->width) * frame->height * 4) {
        d2d_->menu_motion_last_ready_ns = now_ns;
        if (!stats.has_frame || stats.frame_timestamp_100ns != frame->timestamp_100ns) {
            auto& bitmap = d2d_->menu_motion_bitmap;
            const auto size = bitmap ? bitmap->GetPixelSize() : D2D1::SizeU(0, 0);
            HRESULT uploaded = E_FAIL;
            if (bitmap && size.width == frame->width && size.height == frame->height) {
                uploaded = bitmap->CopyFromMemory(nullptr, frame->bgra.data(), frame->width * 4);
            } else {
                const auto properties = D2D1::BitmapProperties(
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                uploaded = d2d_->d2d_context->CreateBitmap(D2D1::SizeU(frame->width, frame->height),
                    frame->bgra.data(), frame->width * 4, &properties, bitmap.ReleaseAndGetAddressOf());
            }
            if (SUCCEEDED(uploaded)) {
                stats.has_frame = true;
                stats.frame_timestamp_100ns = frame->timestamp_100ns;
                ++stats.uploaded_frames;
            } else {
                stats.failed = true;
            }
        }
    }
    if (stats.failed || (!stats.has_frame && now_ns - d2d_->menu_motion_first_request_ns >= decode_timeout_ns) ||
        (stats.has_frame && !reduced_motion && now_ns - d2d_->menu_motion_last_ready_ns >= decode_timeout_ns)) {
        // Keep the last valid still on a mid-loop failure. No worker remains
        // alive retrying a missing codec or repeatedly failed decode.
        stats.failed = true;
        d2d_->menu_motion_failed_assets[asset_index] = true;
        d2d_->menu_motion_decoder->clear();
        d2d_->menu_motion_decoder.reset();
        stats.decoder_active = false;
        return;
    }
    if (!stats.has_frame) return;
    const int64_t delta_ns = previously_ready ? std::max<int64_t>(0, now_ns - d2d_->menu_motion_clock_ns) : 0;
    d2d_->menu_motion_clock_ns = now_ns;
    if (reduced_motion) {
        // An already published still remains visible without further requests.
        // Reset the stall watchdog while stopped so resuming is not a failure.
        d2d_->menu_motion_last_ready_ns = now_ns;
        return;
    }
    stats.position_ns = (stats.position_ns + delta_ns % loop_ns) % loop_ns;
    const int64_t tick = stats.position_ns * stats.frame_rate / 1'000'000'000LL;
    if (tick != d2d_->menu_motion_request_tick) {
        d2d_->menu_motion_request_tick = tick;
        d2d_->menu_motion_decoder->request(d2d_->menu_motion_path, static_cast<double>(tick) / stats.frame_rate);
        ++stats.decode_requests;
    }
}
