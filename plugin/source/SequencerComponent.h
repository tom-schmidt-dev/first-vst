#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../dsp/include/SequencerEngine.hpp"

class SequencerCanvas : public juce::Component, public juce::FileDragAndDropTarget {
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

    void setCurrentCurveType(SegmentCurveType type) {
        mCurrentCurveType = type;
    }

    SegmentCurveType getCurrentCurveType() const noexcept {
        return mCurrentCurveType;
    }

    void updateCanvasWidth(int availableViewportWidth = 0) {
        const int steps = mEngine.getStepCount();
        const int minStepW = (steps == 32) ? 38 : 48;
        const int neededWidth = steps * minStepW;
        const int finalWidth = std::max(availableViewportWidth, neededWidth);
        setSize(finalWidth, std::max(140, getHeight()));
        repaint();
    }

    // --- FileDragAndDropTarget ---
    bool isInterestedInFileDrag(const juce::StringArray& files) override {
        for (const auto& f : files) {
            if (f.endsWithIgnoreCase(".wav")  ||
                f.endsWithIgnoreCase(".aif")  ||
                f.endsWithIgnoreCase(".aiff") ||
                f.endsWithIgnoreCase(".flac")) {
                return true;
            }
        }
        return false;
    }

    void fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/) override {
        mIsDraggingOver = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray& /*files*/) override {
        mIsDraggingOver = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/) override {
        mIsDraggingOver = false;
        for (const auto& f : files) {
            juce::File file(f);
            if (file.existsAsFile()) {
                juce::AudioFormatManager formatMgr;
                formatMgr.registerBasicFormats();
                formatMgr.registerFormat(new juce::FlacAudioFormat(), true);
                if (mEngine.loadSampleFile(file, formatMgr)) {
                    repaint();
                    break;
                }
            }
        }
    }

    void paint(juce::Graphics& g) override {
        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());
        const int steps = mEngine.getStepCount();

        // 1. Mattschwarzer Hintergrund (#0E1013)
        g.fillAll(juce::Colour(0xff0e1013));

        // 2. Horizontale Hilfslinien
        g.setColour(juce::Colour(0xff1a1e26));
        for (float normY : {0.25f, 0.50f, 0.75f}) {
            const float y = h * (1.0f - normY);
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
        }

        // 3. Vertikales Taktraster
        const float stepWidth = w / static_cast<float>(steps);
        for (int s = 0; s <= steps; ++s) {
            const float x = static_cast<float>(s) * stepWidth;
            if (s % 4 == 0) {
                g.setColour(juce::Colour(0xff2a313d));
                g.drawLine(x, 0.0f, x, h, 1.5f);
            } else {
                g.setColour(juce::Colour(0xff161920));
                g.drawLine(x, 0.0f, x, h, 1.0f);
            }
        }

        // 4. Geladene Sample-Wellenform semi-transparent im Hintergrund rendern
        if (mEngine.hasSample()) {
            const auto& sampleBuf = mEngine.getSampleBuffer();
            const int numSamps = sampleBuf.getNumSamples();
            if (numSamps > 0) {
                const float* samps = sampleBuf.getReadPointer(0);
                const float halfH = h * 0.5f;
                const int numBins = std::min<int>(getWidth(), 1024);

                g.setColour(juce::Colour(0x2800e5ff)); // Cyan mit 16% Transparenz
                juce::Path wavePath;

                for (int bin = 0; bin < numBins; ++bin) {
                    const int s0 = static_cast<int>(static_cast<double>(bin) / static_cast<double>(numBins) * numSamps);
                    const int s1 = static_cast<int>(static_cast<double>(bin + 1) / static_cast<double>(numBins) * numSamps);
                    float minVal = 0.0f;
                    float maxVal = 0.0f;

                    for (int s = s0; s < s1 && s < numSamps; ++s) {
                        minVal = std::min(minVal, samps[s]);
                        maxVal = std::max(maxVal, samps[s]);
                    }

                    const float bx = (static_cast<float>(bin) / static_cast<float>(numBins)) * w;
                    const float yTop = halfH - (maxVal * halfH * 0.85f);
                    const float yBottom = halfH - (minVal * halfH * 0.85f);

                    g.drawVerticalLine(static_cast<int>(bx), yTop, yBottom);
                }
            }
        }

        // 5. Inaktive Kurven dezent im Hintergrund rendern
        for (size_t p = 0; p < kNumTrackedParams; ++p) {
            if (p == mSelectedParam || !mVisible[p]) continue;

            const bool isAuto = mEngine.isAutomated(p);
            const float alpha = isAuto ? 0.35f : 0.12f;
            g.setColour(kTrackedParams[p].colour.withAlpha(alpha));

            const auto& nodes = mEngine.getNodes(p);
            drawCurvePath(g, nodes, w, h, 1.2f);
        }

        // 6. Aktuell ausgewählte Kurve leuchtend im Vordergrund rendern
        if (mVisible[mSelectedParam]) {
            const bool isAuto = mEngine.isAutomated(mSelectedParam);
            const float alpha = isAuto ? 1.0f : 0.40f;
            g.setColour(kTrackedParams[mSelectedParam].colour.withAlpha(alpha));

            const auto& nodes = mEngine.getNodes(mSelectedParam);
            drawCurvePath(g, nodes, w, h, 2.5f);

            // Stützpunkte und Spannungsgriffe (Tension Handles) zeichnen
            for (size_t i = 0; i < nodes.size(); ++i) {
                const float nx = nodes[i].x * w;
                const float ny = (1.0f - nodes[i].y) * h;

                // Spannungsgriff in Segmentmitte zeichnen
                if (i + 1 < nodes.size()) {
                    const auto& p0 = nodes[i];
                    const auto& p1 = nodes[i + 1];
                    const float midX = (p0.x + p1.x) * 0.5f * w;
                    const float midY = (1.0f - interpolateSegment(0.5f, p0.y, p1.y, p0.curve, p0.tension)) * h;

                    g.setColour(kTrackedParams[mSelectedParam].colour.withAlpha(0.6f));
                    g.fillRect(midX - 3.0f, midY - 3.0f, 6.0f, 6.0f);
                }

                // Hauptknoten
                g.setColour(juce::Colours::white);
                g.fillEllipse(nx - 4.5f, ny - 4.5f, 9.0f, 9.0f);
                g.setColour(kTrackedParams[mSelectedParam].colour);
                g.drawEllipse(nx - 4.5f, ny - 4.5f, 9.0f, 9.0f, 1.8f);
            }
        }

        // 7. Drag-and-Drop Hervorhebung
        if (mIsDraggingOver) {
            g.setColour(juce::Colour(0x8000e5ff));
            g.drawRect(getLocalBounds(), 2);
            g.setFont(juce::FontOptions{14.0f, juce::Font::bold});
            g.drawText("Drop Audio File (WAV, AIFF, FLAC)", getLocalBounds(), juce::Justification::centred, true);
        }

        // 8. DAW-Playhead Cursor (#FFD700)
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

        // Rechtsklick: Knoten entfernen
        if (e.mods.isRightButtonDown()) {
            if (mEngine.removeNode(mSelectedParam, normX, 0.035f)) {
                repaint();
            }
            return;
        }

        // Prüfe Klick auf Spannungsgriff
        const auto& nodes = mEngine.getNodes(mSelectedParam);
        for (size_t i = 0; i + 1 < nodes.size(); ++i) {
            const float midX = (nodes[i].x + nodes[i + 1].x) * 0.5f;
            const float midY = interpolateSegment(0.5f, nodes[i].y, nodes[i + 1].y, nodes[i].curve, nodes[i].tension);
            if (std::abs(normX - midX) < 0.025f && std::abs(normY - midY) < 0.045f) {
                mActiveTensionSegment = static_cast<int>(i);
                mLastTensionY = e.position.y;
                return;
            }
        }
        mActiveTensionSegment = -1;

        // Knoten hinzufügen oder verschieben
        const float snappedX = snapToGrid(normX);
        mEngine.addOrMoveNode(mSelectedParam, snappedX, normY, mCurrentCurveType, 0.0f);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (e.mods.isRightButtonDown()) return;

        const float w = static_cast<float>(getWidth());
        const float h = static_cast<float>(getHeight());

        if (mActiveTensionSegment >= 0) {
            // Spannungsbiegung (Tension) anpassen
            const float dy = mLastTensionY - e.position.y;
            mLastTensionY = e.position.y;
            const auto& nodes = mEngine.getNodes(mSelectedParam);
            if (static_cast<size_t>(mActiveTensionSegment) < nodes.size()) {
                float tens = nodes[static_cast<size_t>(mActiveTensionSegment)].tension + (dy * 0.02f);
                mEngine.setSegmentTension(mSelectedParam, static_cast<size_t>(mActiveTensionSegment), tens);
                repaint();
            }
            return;
        }

        const float normX = std::clamp(e.position.x / w, 0.0f, 1.0f);
        const float normY = std::clamp(1.0f - (e.position.y / h), 0.0f, 1.0f);

        const float snappedX = snapToGrid(normX);
        mEngine.addOrMoveNode(mSelectedParam, snappedX, normY, mCurrentCurveType, 0.0f);
        repaint();
    }

    void mouseUp(const juce::MouseEvent& /*e*/) override {
        mActiveTensionSegment = -1;
    }

private:
    void drawCurvePath(juce::Graphics& g, const std::vector<SequencerNode>& nodes, float w, float h, float strokeWidth) {
        if (nodes.size() < 2) return;

        juce::Path path;
        path.startNewSubPath(nodes.front().x * w, (1.0f - nodes.front().y) * h);

        for (size_t i = 0; i + 1 < nodes.size(); ++i) {
            const auto& p0 = nodes[i];
            const auto& p1 = nodes[i + 1];
            const int subPoints = 24;

            for (int s = 1; s <= subPoints; ++s) {
                const float u = static_cast<float>(s) / static_cast<float>(subPoints);
                const float currX = (p0.x + u * (p1.x - p0.x)) * w;
                const float currY = (1.0f - interpolateSegment(u, p0.y, p1.y, p0.curve, p0.tension)) * h;
                path.lineTo(currX, currY);
            }
        }

        g.strokePath(path, juce::PathStrokeType(strokeWidth));
    }

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
    SegmentCurveType mCurrentCurveType = SegmentCurveType::Linear;
    int mActiveTensionSegment = -1;
    float mLastTensionY = 0.0f;
    bool mIsDraggingOver = false;
};