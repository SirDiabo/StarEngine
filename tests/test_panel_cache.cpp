#include "TestHelper.h"
#include "Editor/PanelCachePolicy.h"

namespace {

PanelCachePolicy::Input Reusable() {
    PanelCachePolicy::Input in;
    in.enabled = true;
    in.refreshInterval = 0.1f;
    in.surfaceValid = true;
    in.geometryChanged = false;
    in.framesSinceShown = 1;
    in.dirtyAllFrames = 0;
    in.mouseInside = false;
    in.mouseWasInside = false;
    in.focused = false;
    in.secondsSinceRender = 0.02;
    return in;
}

}

TEST(reusable_baseline) {
    CHECK(PanelCachePolicy::CanReuse(Reusable()));
}

TEST(each_blocker_forces_redraw) {
    PanelCachePolicy::Input in;

    in = Reusable(); in.enabled = false;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.refreshInterval = -1.0f;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.surfaceValid = false;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.geometryChanged = true;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.framesSinceShown = 2;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.framesSinceShown = 0;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.dirtyAllFrames = 1;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.mouseInside = true;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.mouseWasInside = true;
    CHECK(!PanelCachePolicy::CanReuse(in));

    in = Reusable(); in.focused = true;
    CHECK(!PanelCachePolicy::CanReuse(in));
}

TEST(refresh_interval_boundary) {
    PanelCachePolicy::Input in = Reusable();
    in.refreshInterval = 0.1f;
    in.secondsSinceRender = 0.0999;
    CHECK(PanelCachePolicy::CanReuse(in));
    in.secondsSinceRender = 0.1001;
    CHECK(!PanelCachePolicy::CanReuse(in));
    in.refreshInterval = 0.0f;
    in.secondsSinceRender = 0.0;
    CHECK(!PanelCachePolicy::CanReuse(in));
}

TEST(activity_holds_redraw_then_decays) {
    int dirty = 0;
    dirty = PanelCachePolicy::NextDirtyAllFrames(dirty, true);
    CHECK(dirty == PanelCachePolicy::kHoldFramesAfterActivity);
    CHECK(PanelCachePolicy::kHoldFramesAfterActivity >= 3);

    int redrawFrames = 0;
    for (int frame = 0; frame < 10; frame++) {
        dirty = PanelCachePolicy::NextDirtyAllFrames(dirty, false);
        PanelCachePolicy::Input in = Reusable();
        in.dirtyAllFrames = dirty;
        if (!PanelCachePolicy::CanReuse(in)) redrawFrames++;
    }
    CHECK(redrawFrames == PanelCachePolicy::kHoldFramesAfterActivity - 1);
    CHECK(dirty == 0);

    dirty = PanelCachePolicy::NextDirtyAllFrames(dirty, false);
    CHECK(dirty == 0);

    dirty = PanelCachePolicy::NextDirtyAllFrames(2, true);
    CHECK(dirty == PanelCachePolicy::kHoldFramesAfterActivity);
}

TEST_MAIN()
