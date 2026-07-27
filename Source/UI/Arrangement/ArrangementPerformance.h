#pragma once

/** Performance-sensitive arrangement timeline paint/update paths.
    Visual changes should preserve these patterns:

    Hot paths (avoid per-frame allocations / full rebuilds):
    - TrackLaneComponent::paint          — lane background via LaneBackgroundCache::renderOrFetch
    - ClipComponent::paint               — per visible clip; gated by TimelineLOD
    - LaneBackgroundCache::ensureImage   — CPU image cache keyed by track/zoom/view/grid/theme
    - TimelineComponent::refreshVisibleTracks — vertical virtualization (visible rows ± margin)
    - TrackLaneComponent::updateClipBounds    — horizontal clip culling + thumbnail hold/release
    - PlayheadOverlay::updateFromTransport    — repaint only when X changes
    - TimelineComponent::paintOverChildren    — transient drag/marquee/time-selection overlays (single pass)

    Invalidation triggers:
    - Zoom/scroll/grid: invalidateLaneBackgrounds() → LaneBackgroundCache::invalidateAll()
    - Theme change: cache key includes theme; stale entries are not reused
    - Tempo map: repaintGrid() + refreshLaneLayouts()

    Overlay policy: drag ghosts, snap guides, clip marquee, and time×track selection are painted once on
    TimelineComponent::paintOverChildren — never as per-clip or per-lane child paint.
*/
