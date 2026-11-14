// Decoration for the dark interior of the bundled Song Select loop only.
// Draw before every UI panel/text pass; custom skins and Result keep their art.
// These small, stylized asterisms follow the supplied visual reference, not an
// astronomical coordinate catalogue. No texture, decoder or per-frame allocation.
if (d2d_->native_menu_brush) {
    struct SkyStar { float x, y, brightness; };
    struct SkyLink { unsigned from, to; };
    // Aries, Libra and Cancer. Coordinates use the existing 1920x1080 UI canvas,
    // keeping their aspect and scale when the viewport is resized or letterboxed.
    constexpr SkyStar aries[] = {
        {0, 26, .65f}, {17, 42, .75f}, {79, 70, 1}, {86, 99, .65f},
        {77, 111, .7f}, {48, 25, .65f}, {25, -9, .9f}};
    constexpr SkyLink aries_links[] = {{0,1},{1,2},{2,3},{3,4},{2,5},{5,6}};
    constexpr SkyStar libra[] = {
        {0, 104, .65f}, {17, 83, .55f}, {23, 61, .6f}, {17, 10, 1},
        {77, 14, .85f}, {110, 87, 1}, {77, 125, .65f}, {85, 145, .5f}};
    constexpr SkyLink libra_links[] = {{0,1},{1,2},{2,3},{3,4},{4,5},{3,5},{5,6},{6,7}};
    constexpr SkyStar cancer[] = {
        {43, 0, .9f}, {46, 37, .6f}, {41, 59, 1}, {10, 93, .75f}, {88, 120, .85f}};
    constexpr SkyLink cancer_links[] = {{0,1},{1,2},{2,3},{2,4}};
    // The native clock already obeys OS/app reduced motion. Brightness changes
    // slowly, by only 8%; stars never flash, drift across labels or reset the video.
    const double sky_seconds = native_menu_motion_.seconds();
    auto sky_star = [&](D2D1_POINT_2F point, float brightness, bool primary) {
        const float core = primary ? 1.25f : .65f;
        if (primary) {
            ctx->FillEllipse(D2D1::Ellipse(point, 7, 7), semantic_color(0x8AAEDA, brightness * .025f));
            ctx->FillEllipse(D2D1::Ellipse(point, 3.5f, 3.5f), semantic_color(0xB9D5F4, brightness * .065f));
            ctx->DrawLine(D2D1::Point2F(point.x - 4, point.y), D2D1::Point2F(point.x + 4, point.y),
                          semantic_color(0xD6E5FA, brightness * .15f), .7f);
            ctx->DrawLine(D2D1::Point2F(point.x, point.y - 4), D2D1::Point2F(point.x, point.y + 4),
                          semantic_color(0xD6E5FA, brightness * .15f), .7f);
        }
        ctx->FillEllipse(D2D1::Ellipse(point, core, core), semantic_color(0xE6EEFA, brightness * .48f));
    };
    auto constellation = [&](const auto& stars, const auto& links, float x, float y, float phase) {
        const float light = .92f + .08f * static_cast<float>(std::sin(sky_seconds * .28 + phase));
        for (const auto& link : links) {
            const auto& a = stars[link.from];
            const auto& b = stars[link.to];
            ctx->DrawLine(D2D1::Point2F(x + a.x, y + a.y), D2D1::Point2F(x + b.x, y + b.y),
                          semantic_color(0xA4BDD8, .13f * light), .75f);
        }
        for (const auto& star : stars)
            sky_star(D2D1::Point2F(x + star.x, y + star.y), star.brightness * light, true);
        // A sparse local field gives depth without filling the whole menu with
        // particles. Integer positions are stable across frames and machines.
        for (unsigned i = 0; i < 12; ++i) {
            const float sx = static_cast<float>((i * 73 + 19) % 193) - 43;
            const float sy = static_cast<float>((i * 47 + 31) % 179) - 29;
            sky_star(D2D1::Point2F(x + sx, y + sy), (.18f + (i % 3) * .08f) * light, false);
        }
    };
    constellation(aries, aries_links, 659, 300, .4f);
    constellation(libra, libra_links, 581, 647, 2.3f);
    constellation(cancer, cancer_links, 1464, 401, 4.1f);
}
