#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../dsp/include/SequencerEngine.hpp"

class SequencerCanvas : public juce::Component {
public:
    explicit SequencerCanvas(SequencerEngine& engine) : mEngine(engine) {
        mVisible.fill(true);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    juce::MouseCursor getMouseCursor() override {
        return juce::MouseCursor::NormalCursor;
    }

    void setSelectedParam(size_t idx) {
        if (idx < kNumTrackedParams) {
            mSelectedParam = idx;
            repaint();
        }
    }

    size_t getSelectedParam() const noexcept { return mSelectedParam; }

    void setParamVisible(size_t idx, bool v) {
        if (idx < kNumTrackedParams) {
            mVisible[idx] = v;
            repaint();
        }
    }

    bool isParamVisible(size_t idx) const noexcept {
        return (idx < kNumTrackedParams) ? mVisible[idx] : false;
    }

    void setGridSnap(int snap) {
        mGridSnap = snap;
    }

    void updateCanvasWidth(int availableViewportWidth = 0) {
        const int steps = mEngine.getStepCount();
        const int minStepW = (steps == 32) ? 36 : 45;
        const int neededWidth = steps * minStepW;
        const int finalWidth = std::max(availableViewportWidth, neededWidth);
        setSize(finalWidth, std::max(120, getHeight()));
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        const int steps = mEngine.getStepCount();

        g.fillAll(juce::Colour(0xff121417));

        g.setColour(juce::Colour(0xff1e222a));
        for (float normY : {0.25f, 0.50f, 0.75f}) {
            const float y = h * (1.0f - normY);
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
        }

        const float stepWidth = w / static_cast<float>(steps);
        for (int s = 0; s <= steps; ++s) {
            const float x = static_cast<float>(s) * stepWidth;
            if (s % 4 == 0) {
                g.setColour(juce::Colour(0xff323a48));
                g.drawLine(x, 0.0f, x, h, 1.5f);
            } else {
                g.setColour(juce::Colour(0xff1e222a));
                g.drawLine(x, 0.0f, x, h, 1.0f);
            }
        }

        for (size_t p = 0; p < kNumTrackedParams; ++p) {
            if (p == mSelectedParam || !mVisible[p]) continue;

            const bool isAuto = mEngine.isAutomated(p);
            const float alpha = isAuto ? 1.0f : 0.20f;
            g.setColour(kTrackedParams[p].colour.withAlpha(alpha));

            juce::Path path;
            const auto& nodes = mEngine.getNodes(p);
            for (size_t i = 0; i < nodes.size(); ++i) {
                const float nx = nodes[i].x * w;
                const float ny = (1.0f - nodes[i].y) * h;
                if (i == 0) path.startNewSubPath(nx, ny);
                else path.lineTo(nx, ny);
            }
            g.strokePath(path, juce::PathStrokeType(isAuto ? 2.0f : 1.2f));
        }

        if (mVisible[mSelectedParam]) {
            const bool isAuto = mEngine.isAutomated(mSelectedParam);
            const float alpha = isAuto ? 1.0f : 0.20f;
            g.setColour(kTrackedParams[mSelectedParam].colour.withAlpha(alpha));

            juce::Path path;
            const auto& nodes = mEngine.getNodes(mSelectedParam);
            for (size_t i = 0; i < nodes.size(); ++i) {
                const float nx = nodes[i].x * w;
                const float ny = (1.0f - nodes[i].y) * h;
                if (i == 0) path.startNewSubPath(nx, ny);
                else path.lineTo(nx, ny);
            }
            g.strokePath(path, juce::PathStrokeType(2.5f));

            for (const auto& node : nodes) {
                const float nx = node.x * w;
                const float ny = (1.0f - node.y) * h;
                g.setColour(juce::Colours::white);
                g.fillEllipse(nx - 4.0f, ny - 4.0f, 8.0f, 8.0f);
                g.setColour(kTrackedParams[mSelectedParam].colour);
                g.drawEllipse(nx - 4.0f, ny - 4.0f, 8.0f, 8.0f, 1.5f);
            }
        }

        const float phase = mEngine.getCurrentPhase();
        const float cursorX = phase * w;
        g.setColour(juce::Colour(0xffffd700));
        g.drawLine(cursorX, 0.0f, cursorX, h, 2.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        const float normX = std::clamp(e.position.x / w, 0.0f, 1.0f);
        const float normY = std::clamp(1.0f - (e.position.y / h), 0.0f, 1.0f);

        if (e.mods.isRightButtonDown()) {
            if (mEngine.removeNode(mSelectedParam, normX, 0.03f)) {
                repaint();
            }
            return;
        }

        const float snappedX = snapToGrid(normX);
        mEngine.addOrMoveNode(mSelectedParam, snappedX, normY);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (e.mods.isRightButtonDown()) return;

        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        const float normX = std::clamp(e.position.x / w, 0.0f, 1.0f);
        const float normY = std::clamp(1.0f - (e.position.y / h), 0.0f, 1.0f);

        const float snappedX = snapToGrid(normX);
        mEngine.addOrMoveNode(mSelectedParam, snappedX, normY);
        repaint();
    }

private:
    float snapToGrid(float rawX) const noexcept {
        if (mGridSnap <= 0) return rawX;
        const float gridSteps = static_cast<float>(mEngine.getStepCount() * (mGridSnap / 4));
        if (gridSteps < 1.0f) return rawX;
        return std::round(rawX * gridSteps) / gridSteps;
    }

    SequencerEngine& mEngine;
    size_t mSelectedParam = 0;
    std::array<bool, kNumTrackedParams> mVisible;
    int mGridSnap = 16;
};