void MenuWindow::pump_messages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            should_close_.store(true, std::memory_order_release);
            return;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void MenuWindow::apply_pending_config() {
    MenuWindowConfig pending;
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        if (!config_dirty_) {
            return;
        }
        pending = pending_config_;
        config_dirty_ = false;
    }

    if (!initialized_.load(std::memory_order_acquire)) {
        std::cerr << "[MenuWindow::apply_pending_config] initializing window..." << std::endl;
        init_done_.store(false, std::memory_order_release);
        init_success_.store(false, std::memory_order_release);
        const bool ok = initialize(pending);
        init_success_.store(ok, std::memory_order_release);
        init_done_.store(true, std::memory_order_release);
        if (!ok) {
            std::cerr << "[MenuWindow::apply_pending_config] initialize FAILED" << std::endl;
            should_close_.store(true, std::memory_order_release);
            shutdown();
        } else {
            std::cerr << "[MenuWindow::apply_pending_config] initialize OK, initialized_="
                      << initialized_.load(std::memory_order_acquire) << std::endl;
        }
        return;
    }

    const MenuWindowConfig previous = config_;
    config_ = pending;

    if (hwnd_ && previous.title != config_.title) {
        const std::wstring title = to_wide(config_.title);
        SetWindowTextW(static_cast<HWND>(hwnd_), title.c_str());
    }

    const bool display_mode_changed = previous.display_mode != config_.display_mode;
    const bool refresh_changed = previous.refresh_hz != config_.refresh_hz;
    const bool resolution_changed = previous.width != config_.width || previous.height != config_.height;

    if (d2d_ && d2d_->swap_chain && (display_mode_changed || refresh_changed || resolution_changed)) {
        const HWND hwnd = static_cast<HWND>(hwnd_);
        const MonitorDisplayInfo monitor = query_monitor_display_info(hwnd);
        UINT next_width = width_;
        UINT next_height = height_;
        int next_x = monitor.rect.left;
        int next_y = monitor.rect.top;
        resolve_window_bounds(config_, monitor, next_width, next_height, next_x, next_y);
        const DWORD next_style = window_style_for_display_mode(config_.display_mode);
        const DWORD next_ex_style = window_ex_style_for_display_mode(config_.display_mode);
        const SIZE next_window_size = window_size_for_client_area(next_width, next_height, next_style, next_ex_style);

        const bool need_fullscreen_reset = fullscreen_ && (resolution_changed || display_mode_changed);
        if (need_fullscreen_reset || (fullscreen_ && config_.display_mode != "fullscreen")) {
            const HRESULT leave_fullscreen_hr = d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
            if (FAILED(leave_fullscreen_hr)) {
                std::cerr << "[MenuWindow::apply_pending_config] SetFullscreenState(FALSE) before resize/mode switch failed hr=0x"
                          << std::hex << static_cast<unsigned long>(leave_fullscreen_hr) << std::dec << std::endl;
                if (config_.display_mode == "fullscreen") {
                    fullscreen_restore_pending_ = true;
                } else {
                    config_ = previous;
                }
                return;
            }
            fullscreen_ = false;
            fullscreen_restore_pending_ = false;
        }
        if (hwnd) {
            if (display_mode_changed) {
                SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(next_style));
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, static_cast<LONG_PTR>(next_ex_style));
            }
            if (previous.display_mode == "windowed" && config_.display_mode == "windowed") {
                RECT current_rect{};
                if (GetWindowRect(hwnd, &current_rect)) {
                    next_x = current_rect.left;
                    next_y = current_rect.top;
                }
            }
            SetWindowPos(hwnd,
                         nullptr,
                         next_x,
                         next_y,
                         next_window_size.cx,
                         next_window_size.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        }

        const bool want_fullscreen = (config_.display_mode == "fullscreen");
        if ((resolution_changed || display_mode_changed) && !want_fullscreen) {
            if (!resize_swap_chain(next_width, next_height)) {
                if (config_.display_mode == "fullscreen") {
                    fullscreen_restore_pending_ = true;
                }
                return;
            }
        } else {
            width_ = next_width;
            height_ = next_height;
            update_layout();
        }

        const bool fullscreen_state_changed = (want_fullscreen != fullscreen_);
        if (fullscreen_state_changed) {
            if (want_fullscreen) {
                if (!enter_fullscreen_mode(next_width, next_height, "MenuWindow::apply_pending_config")) {
                    return;
                }
            } else {
                const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(FALSE, nullptr);
                if (FAILED(fs_hr)) {
                    std::cerr << "[MenuWindow::apply_pending_config] SetFullscreenState(FALSE) failed hr=0x"
                              << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
                    config_.display_mode = previous.display_mode;
                } else {
                    fullscreen_ = false;
                    fullscreen_restore_pending_ = false;
                }
            }
        }
        if (fullscreen_) {
            if (!fullscreen_state_changed) {
                apply_fullscreen_target(d2d_->swap_chain.Get(), config_, width_, height_);
                if (want_fullscreen && (resolution_changed || display_mode_changed)) {
                    if (!resize_swap_chain(next_width, next_height)) {
                        return;
                    }
                }
            }
        }
    }
}

void MenuWindow::update_layout() {
    const float width = static_cast<float>(width_);
    const float height = static_cast<float>(height_);
    const float scale_x = width / kBaseWidth;
    const float scale_y = height / kBaseHeight;
    scale_ = std::min(scale_x, scale_y);
    if (scale_ <= 0.0f) {
        scale_ = 1.0f;
    }
    offset_x_ = (width - kBaseWidth * scale_) * 0.5f;
    offset_y_ = (height - kBaseHeight * scale_) * 0.5f;
}

void MenuWindow::update_brushes() {
    if (!d2d_ || !d2d_->d2d_context) {
        return;
    }

    invalidate_gameplay_note_sprite_cache();
    invalidate_gameplay_static_cache();

    auto* ctx = d2d_->d2d_context.Get();
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xE8ECF1), &d2d_->text_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x6EE7F2), &d2d_->accent_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xFF4D6D, 0.38f), &d2d_->judgement_line_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x9AA3AD), &d2d_->muted_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x1F2130), &d2d_->card_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x14141C, 0.72f), &d2d_->panel_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x0B0B10, 0.75f), &d2d_->footer_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x242638), &d2d_->button_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x6EE7F2, 0.22f), &d2d_->button_selected_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0x31344A), &d2d_->button_border_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xF6F8FF, 0.85f), &d2d_->lane_divider_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xF6F8FF, 0.97f), &d2d_->note_fill_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xCAD8E7, 0.98f), &d2d_->note_border_brush);
    ctx->CreateSolidColorBrush(D2D1::ColorF(0xF6F8FF, 0.36f), &d2d_->note_hold_brush);

    {
        D2D1_GRADIENT_STOP stops[2]{};
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(0x6EE7F2, 0.32f);
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(0x0B0B10, 0.0f);
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, &d2d_->glow_stops))) {
            const D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::RadialGradientBrushProperties(
                    D2D1::Point2F(kBaseWidth * 0.5f, kBaseHeight * 0.55f),
                    D2D1::Point2F(0.0f, 0.0f),
                    900.0f,
                    520.0f);
            ctx->CreateRadialGradientBrush(props, d2d_->glow_stops.Get(), &d2d_->glow_brush);
        }
    }

    {
        D2D1_GRADIENT_STOP stops[2]{};
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(0x6EE7F2, 1.0f);
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(0xE8ECF1, 1.0f);
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, &d2d_->logo_stops))) {
            const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(0.0f, 0.0f),
                    D2D1::Point2F(kBaseWidth, 0.0f));
            ctx->CreateLinearGradientBrush(props, d2d_->logo_stops.Get(), &d2d_->logo_brush);
        }
    }

    auto create_button_gradient = [ctx](uint32_t a, uint32_t b,
                                        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection>* stops_out,
                                        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>* brush_out) {
        D2D1_GRADIENT_STOP stops[2]{};
        stops[0].position = 0.0f;
        stops[0].color = D2D1::ColorF(a, 0.95f);
        stops[1].position = 1.0f;
        stops[1].color = D2D1::ColorF(b, 0.95f);
        if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, stops_out->ReleaseAndGetAddressOf()))) {
            const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
                D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(0.0f, 0.0f),
                    D2D1::Point2F(1.0f, 1.0f));
            ctx->CreateLinearGradientBrush(props, stops_out->Get(), brush_out->ReleaseAndGetAddressOf());
        }
    };

    create_button_gradient(0x165CFF, 0x6EE7F2, &d2d_->play_stops, &d2d_->play_brush);
    create_button_gradient(0x7B2CFF, 0xFF60C8, &d2d_->edit_stops, &d2d_->edit_brush);
    create_button_gradient(0xFF8C1A, 0xFF4D6D, &d2d_->options_stops, &d2d_->options_brush);
    create_button_gradient(0xFF2D74, 0xB0003A, &d2d_->exit_stops, &d2d_->exit_brush);

    D2D1_GRADIENT_STOP stops[2]{};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0x0B0B10);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(0x14141C);

    if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 2, &d2d_->bg_stops))) {
        const D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props =
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0.0f, 0.0f),
                D2D1::Point2F(0.0f, static_cast<float>(height_)));
        ctx->CreateLinearGradientBrush(props, d2d_->bg_stops.Get(), &d2d_->bg_brush);
    }
}

void MenuWindow::invalidate_menu_scene_target() {
    if (!d2d_) {
        return;
    }
    d2d_->menu_scene_target_view.Reset();
}

bool MenuWindow::ensure_menu_scene_resources() {
    if (!d2d_ || !d2d_->device) {
        return false;
    }
    if (d2d_->menu_scene_vertex_shader && d2d_->menu_scene_pixel_shader && d2d_->menu_scene_constant_buffer) {
        return true;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vertex_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixel_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    constexpr UINT shader_flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

    const HRESULT vs_hr =
        D3DCompile(kMenuSceneShaderSource,
                   sizeof(kMenuSceneShaderSource) - 1,
                   "MenuScene",
                   nullptr,
                   nullptr,
                   "vs_main",
                   "vs_4_0",
                   shader_flags,
                   0,
                   &vertex_blob,
                   &error_blob);
    if (FAILED(vs_hr)) {
        if (error_blob) {
            std::cerr << "[MenuWindow] Menu-scene VS compile failed: "
                      << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return false;
    }

    error_blob.Reset();
    const HRESULT ps_hr =
        D3DCompile(kMenuSceneShaderSource,
                   sizeof(kMenuSceneShaderSource) - 1,
                   "MenuScene",
                   nullptr,
                   nullptr,
                   "ps_main",
                   "ps_4_0",
                   shader_flags,
                   0,
                   &pixel_blob,
                   &error_blob);
    if (FAILED(ps_hr)) {
        if (error_blob) {
            std::cerr << "[MenuWindow] Menu-scene PS compile failed: "
                      << static_cast<const char*>(error_blob->GetBufferPointer()) << std::endl;
        }
        return false;
    }

    if (FAILED(d2d_->device->CreateVertexShader(vertex_blob->GetBufferPointer(),
                                                vertex_blob->GetBufferSize(),
                                                nullptr,
                                                d2d_->menu_scene_vertex_shader.ReleaseAndGetAddressOf()))) {
        return false;
    }
    if (FAILED(d2d_->device->CreatePixelShader(pixel_blob->GetBufferPointer(),
                                               pixel_blob->GetBufferSize(),
                                               nullptr,
                                               d2d_->menu_scene_pixel_shader.ReleaseAndGetAddressOf()))) {
        d2d_->menu_scene_vertex_shader.Reset();
        return false;
    }

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(MenuSceneConstants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    buffer_desc.CPUAccessFlags = 0;
    buffer_desc.MiscFlags = 0;
    buffer_desc.StructureByteStride = 0;

    if (FAILED(d2d_->device->CreateBuffer(&buffer_desc,
                                          nullptr,
                                          d2d_->menu_scene_constant_buffer.ReleaseAndGetAddressOf()))) {
        d2d_->menu_scene_vertex_shader.Reset();
        d2d_->menu_scene_pixel_shader.Reset();
        return false;
    }

    return true;
}

bool MenuWindow::render_menu_scene(MenuScreenKind kind, int64_t now_ns) {
    if (!menu_scene_enabled(kind) || !d2d_ || !d2d_->context || !d2d_->menu_scene_target_view) {
        return false;
    }
    if (!ensure_menu_scene_resources()) {
        return false;
    }

    MenuSceneConstants constants{};
    constants.resolution[0] = static_cast<float>(std::max(1u, width_));
    constants.resolution[1] = static_cast<float>(std::max(1u, height_));
    constants.time_sec = static_cast<float>(static_cast<double>(now_ns) / 1'000'000'000.0);
    constants.scene_kind = (kind == MenuScreenKind::SongSelect) ? 1.0f : 0.0f;

    if (kind == MenuScreenKind::SongSelect) {
        constants.primary_color[0] = 0.38f;
        constants.primary_color[1] = 0.84f;
        constants.primary_color[2] = 0.98f;
        constants.primary_color[3] = 1.0f;
        constants.secondary_color[0] = 0.56f;
        constants.secondary_color[1] = 0.62f;
        constants.secondary_color[2] = 0.98f;
        constants.secondary_color[3] = 1.0f;
    } else {
        constants.primary_color[0] = 0.25f;
        constants.primary_color[1] = 0.86f;
        constants.primary_color[2] = 0.93f;
        constants.primary_color[3] = 1.0f;
        constants.secondary_color[0] = 1.00f;
        constants.secondary_color[1] = 0.62f;
        constants.secondary_color[2] = 0.30f;
        constants.secondary_color[3] = 1.0f;
    }

    ID3D11DeviceContext* const context = d2d_->context.Get();
    context->UpdateSubresource(d2d_->menu_scene_constant_buffer.Get(), 0, nullptr, &constants, 0, 0);

    const float clear_color[4] = {0.015f, 0.020f, 0.032f, 1.0f};
    context->ClearRenderTargetView(d2d_->menu_scene_target_view.Get(), clear_color);

    ID3D11RenderTargetView* render_target = d2d_->menu_scene_target_view.Get();
    context->OMSetRenderTargets(1, &render_target, nullptr);

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(std::max(1u, width_));
    viewport.Height = static_cast<float>(std::max(1u, height_));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(d2d_->menu_scene_vertex_shader.Get(), nullptr, 0);
    context->PSSetShader(d2d_->menu_scene_pixel_shader.Get(), nullptr, 0);

    ID3D11Buffer* constant_buffer = d2d_->menu_scene_constant_buffer.Get();
    context->VSSetConstantBuffers(0, 1, &constant_buffer);
    context->PSSetConstantBuffers(0, 1, &constant_buffer);
    context->Draw(3, 0);

    ID3D11Buffer* null_buffer = nullptr;
    context->VSSetConstantBuffers(0, 1, &null_buffer);
    context->PSSetConstantBuffers(0, 1, &null_buffer);
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    ID3D11RenderTargetView* null_target = nullptr;
    context->OMSetRenderTargets(1, &null_target, nullptr);
    return true;
}

bool MenuWindow::recreate_targets() {
    if (!d2d_ || !d2d_->swap_chain || !d2d_->d2d_context) {
        return false;
    }

    invalidate_menu_scene_target();
    invalidate_gameplay_note_sprite_cache();
    invalidate_song_select_preview_cache();
    clear_song_card_preview_cache();
    invalidate_gameplay_static_cache();
    d2d_->d2d_context->SetTarget(nullptr);
    d2d_->d2d_target.Reset();

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    const HRESULT buffer_hr = d2d_->swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(buffer_hr)) {
        std::cerr << "[MenuWindow::recreate_targets] GetBuffer failed hr=0x" << std::hex
                  << static_cast<unsigned long>(buffer_hr) << std::dec << std::endl;
        return false;
    }

    if (d2d_->device) {
        const HRESULT rtv_hr =
            d2d_->device->CreateRenderTargetView(back_buffer.Get(),
                                                 nullptr,
                                                 d2d_->menu_scene_target_view.ReleaseAndGetAddressOf());
        if (FAILED(rtv_hr)) {
            std::cerr << "[MenuWindow::recreate_targets] CreateRenderTargetView failed hr=0x" << std::hex
                      << static_cast<unsigned long>(rtv_hr) << std::dec << std::endl;
        }
    }

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(back_buffer.As(&surface))) {
        return false;
    }

    auto try_create = [&](D2D1_ALPHA_MODE alpha_mode, std::string_view label) -> bool {
        D2D1_BITMAP_PROPERTIES1 props =
            D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, alpha_mode),
                96.0f, 96.0f);

        Microsoft::WRL::ComPtr<ID2D1Bitmap1> target;
        const HRESULT hr =
            d2d_->d2d_context->CreateBitmapFromDxgiSurface(surface.Get(), &props, &target);
        if (FAILED(hr)) {
            std::cerr << "[MenuWindow::recreate_targets] CreateBitmapFromDxgiSurface(" << label
                      << ") failed hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec
                      << std::endl;
            return false;
        }

        d2d_->d2d_target = std::move(target);
        d2d_->d2d_context->SetTarget(d2d_->d2d_target.Get());
        return true;
    };

    if (try_create(D2D1_ALPHA_MODE_IGNORE, "ignore")) {
        return true;
    }
    if (try_create(D2D1_ALPHA_MODE_PREMULTIPLIED, "premultiplied")) {
        return true;
    }
    if (try_create(D2D1_ALPHA_MODE_UNKNOWN, "unknown")) {
        return true;
    }
    return false;
}

bool MenuWindow::resize_swap_chain(unsigned int width, unsigned int height) {
    if (!d2d_ || !d2d_->swap_chain || width == 0 || height == 0) {
        return false;
    }

    invalidate_menu_scene_target();
    d2d_->d2d_context->SetTarget(nullptr);
    d2d_->d2d_target.Reset();
    const HRESULT resize_hr =
        d2d_->swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swap_chain_flags_);
    if (FAILED(resize_hr)) {
        std::cerr << "[MenuWindow::resize_swap_chain] ResizeBuffers failed hr=0x"
                  << std::hex << static_cast<unsigned long>(resize_hr) << std::dec
                  << " requested=" << width << "x" << height
                  << " mode=" << config_.display_mode
                  << " fullscreen_=" << fullscreen_ << std::endl;
        if (!recreate_targets()) {
            fail_fatal("Failed to recover the menu render target after a resize transition.");
            shutdown();
            return false;
        }
        const std::uint32_t resize_code = static_cast<std::uint32_t>(resize_hr);
        if (resize_code == kDxgiErrorInvalidCall || resize_code == kDxgiStatusModeChanged) {
            pending_width_ = width;
            pending_height_ = height;
            resize_pending_ = true;
            return false;
        }
        fail_fatal("Failed to resize the menu swap chain. Attach logs/run.log.");
        shutdown();
        return false;
    }
    width_ = width;
    height_ = height;
    if (!recreate_targets()) {
        std::cerr << "[MenuWindow::resize_swap_chain] recreate_targets failed after ResizeBuffers success."
                  << " requested=" << width << "x" << height << std::endl;
        pending_width_ = width;
        pending_height_ = height;
        resize_pending_ = true;
        return false;
    }
    update_layout();
    update_brushes();
    return true;
}

bool MenuWindow::enter_fullscreen_mode(unsigned int width,
                                       unsigned int height,
                                       const char* log_context) {
    if (!d2d_ || !d2d_->swap_chain) {
        return false;
    }

    const HRESULT fs_hr = d2d_->swap_chain->SetFullscreenState(TRUE, nullptr);
    if (FAILED(fs_hr)) {
        std::cerr << "[" << log_context << "] SetFullscreenState(TRUE) failed hr=0x"
                  << std::hex << static_cast<unsigned long>(fs_hr) << std::dec << std::endl;
        fullscreen_restore_pending_ = true;
        return false;
    }

    fullscreen_ = true;
    fullscreen_restore_pending_ = false;
    apply_fullscreen_target(d2d_->swap_chain.Get(), config_, width, height);

    // DXGI flip-model fullscreen transitions require a post-transition ResizeBuffers.
    if (!resize_swap_chain(width, height)) {
        std::cerr << "[" << log_context
                  << "] ResizeBuffers after SetFullscreenState(TRUE) did not complete immediately."
                  << " requested=" << width << "x" << height << std::endl;
        return false;
    }

    return true;
}
