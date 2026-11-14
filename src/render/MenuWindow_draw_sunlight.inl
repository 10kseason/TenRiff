// One shared, static radial light follows real menu hit regions. It never
// registers input or changes selection, and has no timer/reduced-motion pulse.
menu_sunlight_visible_ = false;
if (menu_pointer_inside_ && is_input_foreground() && !transition.active &&
    (data.kind != MenuScreenKind::GameplayHud || data.gameplay.paused) &&
    !(data.generic.skin_preview.visible && data.generic.skin_preview.fullscreen)) {
    float px = 0, py = 0;
    if (translate_window_point(menu_pointer_x_, menu_pointer_y_, &px, &py)) {
        for (auto it = hit_regions_.rbegin(); it != hit_regions_.rend(); ++it) {
            if (px < it->left || px > it->right || py < it->top || py > it->bottom) continue;
            const auto kind = it->kind;
            // Follow the draw stack: only the topmost modal owns hover light.
            // A chat link warning retains the chat underneath; its buttons must
            // not be rejected by the lower chat/account/help filters.
            if (data.url_warning_overlay.visible) {
                if (kind != MenuHitTargetKind::UrlWarningButton) break;
            } else if (data.account_overlay.visible) {
                if (kind != MenuHitTargetKind::AccountPlayMode &&
                    kind != MenuHitTargetKind::SitesLeaderboardAction) break;
            } else if (data.chat_overlay.visible) {
                if (kind != MenuHitTargetKind::ChatUrl) break;
            } else if (data.help_overlay.visible) {
                if (kind != MenuHitTargetKind::GenericHelpPage) break;
            }
            if (kind == MenuHitTargetKind::None || kind == MenuHitTargetKind::MouseWheel ||
                kind == MenuHitTargetKind::FileDrop || kind == MenuHitTargetKind::SongScrollbar ||
                kind == MenuHitTargetKind::GameplayFieldDrag || kind == MenuHitTargetKind::BmsEditorGrid ||
                kind == MenuHitTargetKind::BmsEditorNote || kind == MenuHitTargetKind::BmsEditorBgm ||
                kind == MenuHitTargetKind::SkinPreviewLane) break;
            if (!d2d_->menu_sunlight) {
                const D2D1_GRADIENT_STOP stops[] = {
                    {0, D2D1::ColorF(0xFFF6D5, .30f)},
                    {.15f, D2D1::ColorF(0xFFE6A6, .15f)},
                    {.48f, D2D1::ColorF(0xFFD078, .055f)},
                    {1, D2D1::ColorF(0xFFBD5B, 0)}};
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> colors;
                if (SUCCEEDED(ctx->CreateGradientStopCollection(stops, 4, &colors)))
                    ctx->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(
                        D2D1::Point2F(px, py), D2D1::Point2F(0, 0), 88, 88), colors.Get(),
                        &d2d_->menu_sunlight);
            }
            if (d2d_->menu_sunlight) {
                auto* light = d2d_->menu_sunlight.Get();
                light->SetCenter(D2D1::Point2F(px, py));
                // Low-opacity spill around the pointer, with extra light only
                // on the hovered surface. Text remains readable under the glow.
                light->SetOpacity(.65f);
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 88, 88), light);
                ctx->PushAxisAlignedClip(D2D1::RectF(it->left, it->top, it->right, it->bottom),
                                         D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                light->SetOpacity(.65f);
                ctx->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 88, 88), light);
                ctx->PopAxisAlignedClip();
                menu_sunlight_visible_ = true;
            }
            break;
        }
    }
}
