// Screen test: colour bars, a grey ramp, edge markers and an "up" arrow so we
// can confirm the picture is the right way up and fills the whole screen.
#include "app/app.h"
#include "gfx/font.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace {

class DisplayTestScene : public Scene {
public:
    void update(App& app, const Input& in, uint32_t) override {
        if (in.pressed(Btn::B) || in.pressed(Btn::Menu)) {
            app.pop();
            return;
        }
        if (in.pressed(Btn::A)) app.setFlip(!app.flip());
    }

    void draw(App& app, Canvas& c, uint32_t) override {
        c.clear(rgb(16, 18, 26));
        // colour bars
        static const uint32_t bars[8] = {rgb(255, 255, 255), rgb(255, 255, 0), rgb(0, 255, 255), rgb(0, 255, 0),
                                         rgb(255, 0, 255),   rgb(255, 0, 0),   rgb(0, 0, 255),   rgb(0, 0, 0)};
        for (int i = 0; i < 8; ++i) c.fillRect(i * 80, 0, 80, 120, bars[i]);
        // grey ramp and pure channel ramps
        for (int x = 0; x < 640; ++x) {
            int v = x * 255 / 639;
            c.fillRect(x, 120, 1, 24, rgb(v, v, v));
            c.fillRect(x, 144, 1, 8, rgb(v, 0, 0));
            c.fillRect(x, 152, 1, 8, rgb(0, v, 0));
            c.fillRect(x, 160, 1, 8, rgb(0, 0, v));
        }
        // edge markers: a 1-pixel border, corners labelled
        c.frameRect(0, 0, 640, 480, rgb(255, 255, 255), 1);
        c.frameRect(2, 2, 636, 476, rgb(255, 60, 60), 1);
        drawText(c, FONT_SMB, 8, 182, "TOP LEFT", theme::text);
        drawTextAligned(c, FONT_SMB, 0, 182, 632, Align::Right, "TOP RIGHT", theme::text);
        drawText(c, FONT_SMB, 8, 458, "BOTTOM LEFT", theme::text);
        drawTextAligned(c, FONT_SMB, 0, 458, 632, Align::Right, "BOTTOM RIGHT", theme::text);

        // up arrow
        float xs[7] = {320, 360, 336, 336, 304, 304, 280};
        float ys[7] = {196, 246, 246, 300, 300, 246, 246};
        c.fillPolygon(xs, ys, 7, theme::gold);
        drawTextAligned(c, FONT_MDB, 0, 306, 640, Align::Center, "This arrow should point UP", theme::text);
        drawTextAligned(c, FONT_SM, 0, 326, 640, Align::Center,
                        "and you should see the white border on all four sides.", theme::dim);

        // controls / status
        drawPanel(c, Rect{120, 352, 400, 92}, theme::panel, theme::border, 10);
        std::string status = std::string("Picture turned 180\xC2\xB0: ") + (app.rotateOutput() ? "yes" : "no") +
                             "   (backend: " + app.platform().name() + ")";
        drawTextAligned(c, FONT_SM, 120, 362, 400, Align::Center, status, theme::dim);
        drawTextAligned(c, FONT_MD, 120, 382, 400, Align::Center, keyText("Upside down? Press A to flip it."), theme::text);
        drawTextAligned(c, FONT_SM, 120, 406, 400, Align::Center, keyText("The choice is saved.  B = back"), theme::dim);
        // text samples
        drawText(c, FONT_XS, 130, 426, "Tiny 11px", theme::text);
        drawText(c, FONT_SM, 210, 425, "Small 13px", theme::text);
        drawText(c, FONT_MD, 300, 423, "Medium 16px", theme::text);
        drawText(c, FONT_LG, 424, 420, "Big", theme::gold);
    }
};

}  // namespace

std::unique_ptr<Scene> makeDisplayTestScene() { return std::unique_ptr<Scene>(new DisplayTestScene()); }
