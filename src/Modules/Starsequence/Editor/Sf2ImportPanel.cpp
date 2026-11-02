#include "Sf2ImportPanel.h"
#include "Sf2Match.h"
#include "Sf2Import.h"
#include "SequenceEditorSession.h"
#include "Editor/AssetImporter.h"
#include "Editor/ImportReportPanel.h"
#include "Editor/NativeFileDialog.h"
#include "Editor/UITheme.h"
#include "Core/ContentPathResolver.h"
#include "Core/ImportHandlerRegistry.h"
#include "Core/Project.h"
#include "Core/Widgets/ScissorStack.h"
#include "Core/Widgets/WidgetFont.h"
#include "Core/Widgets/WidgetInput.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace Starsequence;

namespace {

constexpr float kFontSize = 11.0f;
constexpr float kFontSpacing = 1.0f;
constexpr float kRowHeight = 26.0f;
constexpr float kPad = 10.0f;

bool Hover(Rectangle r) {
    const void* scope = WidgetInput::GetInputScope();
    Vector2 m = GetMousePosition();
    if (WidgetInput::IsMouseBlocked()) return false;
    if (scope && WidgetInput::IsMouseOccluded(m, scope)) return false;
    return CheckCollisionPointRec(m, r);
}

void Text(const std::string& text, float x, float y, Color color) {
    DrawTextEx(WidgetFont::Get(), text.c_str(), { x, y }, kFontSize, kFontSpacing, color);
}

float TextWidth(const std::string& text) {
    return MeasureTextEx(WidgetFont::Get(), text.c_str(), kFontSize, kFontSpacing).x;
}

std::string FitRight(const std::string& text, float width) {
    if (TextWidth(text) <= width) return text;
    std::string cut = text;
    while (cut.size() > 4 && TextWidth("..." + cut) > width) cut.erase(cut.begin());
    return "..." + cut;
}

std::string FitLeft(const std::string& text, float width) {
    if (TextWidth(text) <= width) return text;
    std::string cut = text;
    while (cut.size() > 1 && TextWidth(cut + "...") > width) cut.pop_back();
    return cut + "...";
}

bool Button(Rectangle r, const std::string& label, Color fill, bool enabled = true) {
    const UIPalette& theme = UITheme::Get();
    bool hovered = enabled && Hover(r);
    Color base = fill.a == 0 ? theme.buttonBg : fill;
    DrawRectangleRec(r, !enabled ? ColorBrightness(theme.buttonBg, -0.3f) : (hovered ? ColorBrightness(base, 0.15f) : base));
    DrawRectangleLinesEx(r, 1, theme.buttonBorder);
    float tw = TextWidth(label);
    Text(label, r.x + (r.width - tw) / 2.0f, r.y + (r.height - kFontSize) / 2.0f, enabled ? theme.textPrimary : theme.textSecondary);
    return enabled && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

class AssetSampleWriter : public Sf2::SampleWriter {
public:
    AssetSampleWriter(const std::string& folderPath) : folder(folderPath) {
        std::error_code ec;
        temp = fs::temp_directory_path(ec) / "starsequence_sf2_import";
        fs::create_directories(temp, ec);
    }
    ~AssetSampleWriter() override {
        std::error_code ec;
        fs::remove_all(temp, ec);
    }
    std::string Write(const std::string& name, const std::vector<int16_t>& pcm, int sampleRate) override {
        std::vector<uint8_t> wav = Sf2::EncodeWav16(pcm, sampleRate);
        fs::path file = temp / (name + ".wav");
        {
            std::ofstream out(file, std::ios::binary);
            if (!out) return "";
            out.write((const char*)wav.data(), (std::streamsize)wav.size());
            if (!out) return "";
        }
        std::string asset = AssetImporter::Cook(file.string(), "Sound", ".starsound", folder);
        std::error_code ec;
        fs::remove(file, ec);
        if (asset.empty()) return "";
        return ContentPathResolver::MakeRelative(asset);
    }

private:
    std::string folder;
    fs::path temp;
};

}

void Sf2ImportPanel::Initialize() {
    name = "SoundFont Import";
    visible = false;
}

std::string Sf2ImportPanel::InstrumentSignature() const {
    std::string s;
    for (const Instrument& inst : session->GetSequence().instruments) {
        s += inst.name;
        s += '|';
    }
    return s;
}

std::string Sf2ImportPanel::DefaultDestination() const {
    std::error_code ec;
    fs::path sequenceFolder = fs::absolute(fs::path(session->GetPath()), ec).parent_path();
    std::string folderName = Sf2::SanitizeFileName(fs::path(soundFontPath).stem().string(), "SoundFont");
    return (sequenceFolder / folderName).string();
}

void Sf2ImportPanel::ResetDestination() {
    destinationCustom = false;
    destination = loaded ? DefaultDestination() : std::string();
}

std::string Sf2ImportPanel::PresetLabel(int preset) const {
    if (preset < 0 || preset >= soundFont.PresetCount()) return "(none)";
    const Sf2::PresetHeader& p = soundFont.presets[(size_t)preset];
    return p.name.empty() ? "Preset " + std::to_string(preset) : p.name;
}

void Sf2ImportPanel::Open() {
    visible = true;
    picking = false;
    if (loaded) SyncRows();
}

void Sf2ImportPanel::LoadSoundFont(const std::string& path) {
    Sf2::File file;
    std::vector<Sf2::Diagnostic> diagnostics;
    if (!Sf2::LoadSf2File(path, file, diagnostics)) {
        statusIsError = true;
        status = diagnostics.empty() ? std::string("Could not load the SoundFont") : diagnostics.back().text;
        return;
    }
    soundFont = std::move(file);
    soundFontPath = path;
    loaded = true;
    statusIsError = false;
    status = std::to_string(soundFont.PresetCount()) + " presets, " + std::to_string(soundFont.SampleCount()) + " samples";

    sortedPresets.clear();
    for (int i = 0; i < soundFont.PresetCount(); i++) sortedPresets.push_back(i);
    std::stable_sort(sortedPresets.begin(), sortedPresets.end(), [&](int a, int b) {
        const Sf2::PresetHeader& pa = soundFont.presets[(size_t)a];
        const Sf2::PresetHeader& pb = soundFont.presets[(size_t)b];
        bool da = pa.bank == 128, db = pb.bank == 128;
        if (da != db) return !da;
        if (pa.bank != pb.bank) return pa.bank < pb.bank;
        if (pa.program != pb.program) return pa.program < pb.program;
        return pa.name < pb.name;
    });

    if (!destinationCustom) destination = DefaultDestination();
    rows.clear();
    signature.clear();
    SyncRows();
}

void Sf2ImportPanel::AutoMatchAll() {
    const Sequence& seq = session->GetSequence();
    for (Row& row : rows) {
        if (row.instrument < 0 || (size_t)row.instrument >= seq.instruments.size()) continue;
        Sf2::MatchResult match = Sf2::MatchInstrument(soundFont, seq.instruments[(size_t)row.instrument].name);
        row.preset = match.preset;
        row.score = match.score;
        bool hasSamples = false;
        for (const SampleSet& set : seq.instruments[(size_t)row.instrument].sets) if (!set.zones.empty()) hasSamples = true;
        row.include = match.preset >= 0 && !hasSamples;
    }
}

void Sf2ImportPanel::SyncRows() {
    if (!loaded || !session) return;
    std::string current = InstrumentSignature();
    if (current == signature) return;
    signature = current;

    const Sequence& seq = session->GetSequence();
    std::vector<Row> next;
    std::vector<std::string> previousNames;
    for (const Row& r : rows) previousNames.push_back(r.instrument >= 0 ? std::to_string(r.instrument) : std::string());

    for (size_t i = 0; i < seq.instruments.size(); i++) {
        Row row;
        row.instrument = (int)i;
        bool reused = false;
        for (const Row& old : rows) {
            if (old.instrument == (int)i) { row = old; reused = true; break; }
        }
        if (!reused) {
            Sf2::MatchResult match = Sf2::MatchInstrument(soundFont, seq.instruments[i].name);
            row.preset = match.preset;
            row.score = match.score;
            bool hasSamples = false;
            for (const SampleSet& set : seq.instruments[i].sets) if (!set.zones.empty()) hasSamples = true;
            row.include = match.preset >= 0 && !hasSamples;
        }
        next.push_back(row);
    }
    for (const Row& old : rows) if (old.instrument < 0) next.push_back(old);
    rows = next;
}

void Sf2ImportPanel::RunImport() {
    std::error_code ec;
    fs::path contentRoot = fs::path(Project::Get().GetRootPath()) / "Content";
    fs::path dest = fs::absolute(fs::path(destination), ec);
    fs::path relative = fs::relative(dest, contentRoot, ec);
    if (ec || relative.empty() || *relative.begin() == "..") {
        statusIsError = true;
        status = "The samples folder must be inside the project's Content folder";
        return;
    }
    fs::create_directories(dest, ec);
    if (ec) {
        statusIsError = true;
        status = "Could not create the samples folder";
        return;
    }

    int wanted = 0;
    for (const Row& r : rows) if (r.include && r.preset >= 0) wanted++;
    if (wanted == 0) {
        statusIsError = true;
        status = "Nothing to import. Tick an instrument and choose a preset for it";
        return;
    }

    ImportResult report;
    std::vector<Sf2::Message> messages;
    std::map<int, std::string> cache;
    int imported = 0, failed = 0;
    {
        AssetSampleWriter writer(dest.string());
        for (Row& row : rows) {
            if (!row.include || row.preset < 0) continue;
            SampleSet set;
            if (!Sf2::BuildSampleSet(soundFont, row.preset, writer, cache, set, messages)) { failed++; continue; }
            if (row.instrument >= 0) session->ImportSampleSet(row.instrument, set);
            else session->AddInstrumentFromSet(set.name, set);
            imported++;
            row.include = false;
        }
    }
    for (const Sf2::Message& m : messages) {
        if (m.error) report.Error(m.text);
        else report.Warn(m.text);
    }

    rows.erase(std::remove_if(rows.begin(), rows.end(), [](const Row& r) { return r.instrument < 0 && !r.include; }), rows.end());
    signature.clear();
    SyncRows();

    statusIsError = imported == 0;
    status = "Imported " + std::to_string(imported) + (imported == 1 ? " preset" : " presets") + " (" + std::to_string(cache.size()) + " samples)";
    if (failed > 0) status += ", " + std::to_string(failed) + " failed";
    if (!report.messages.empty()) ImportReportPanel::Get().Open("SoundFont Import Report", report.messages);
}

void Sf2ImportPanel::DrawPicker(const Rectangle& area) {
    const UIPalette& theme = UITheme::Get();
    Vector2 mouse = GetMousePosition();

    Rectangle searchBox = { area.x, area.y, area.width - 80.0f, 24.0f };
    Rectangle cancel = { area.x + area.width - 74.0f, area.y, 74.0f, 24.0f };
    DrawRectangleRec(searchBox, theme.fieldBg);
    DrawRectangleLinesEx(searchBox, 1, theme.accent);
    std::string shown = filter.empty() ? std::string("Type to search presets...") : filter + (std::fmod(GetTime(), 1.0) < 0.5 ? "|" : "");
    Text(shown, searchBox.x + 6.0f, searchBox.y + 6.0f, filter.empty() ? theme.textSecondary : theme.textPrimary);

    int ch = GetCharPressed();
    while (ch > 0) {
        if (ch >= 32 && ch < 127 && filter.size() < 48) { filter.push_back((char)ch); pickScroll = 0.0f; }
        ch = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !filter.empty()) { filter.pop_back(); pickScroll = 0.0f; }

    std::vector<int> filtered;
    std::string needle = Lower(filter);
    for (int index : sortedPresets) {
        if (needle.empty() || Lower(soundFont.presets[(size_t)index].name).find(needle) != std::string::npos) filtered.push_back(index);
    }

    bool closePicker = Button(cancel, "Cancel", { 0, 0, 0, 0 });
    if (IsKeyPressed(KEY_ESCAPE)) closePicker = true;

    Rectangle list = { area.x, area.y + 30.0f, area.width, area.height - 30.0f };
    float contentHeight = (float)filtered.size() * kRowHeight;
    float maxScroll = std::max(0.0f, contentHeight - list.height);
    if (Hover(list)) pickScroll -= GetMouseWheelMove() * 40.0f;
    pickScroll = std::clamp(pickScroll, 0.0f, maxScroll);

    int chosen = -1;
    if (IsKeyPressed(KEY_ENTER) && !filtered.empty()) chosen = filtered.front();
    DrawRectangleRec(list, ColorBrightness(theme.panelBg, -0.1f));
    for (size_t i = 0; i < filtered.size(); i++) {
        Rectangle row = { list.x, list.y + (float)i * kRowHeight - pickScroll, list.width, kRowHeight };
        if (row.y + row.height < list.y || row.y > list.y + list.height) continue;
        bool hovered = Hover(row) && CheckCollisionPointRec(mouse, list);
        const Sf2::PresetHeader& p = soundFont.presets[(size_t)filtered[i]];
        if (hovered) DrawRectangleRec(row, theme.surfaceHover);
        std::string name = p.name.empty() ? "Preset " + std::to_string(filtered[i]) : p.name;
        Text(FitLeft(name, row.width - 110.0f), row.x + 8.0f, row.y + (row.height - kFontSize) / 2.0f, theme.textPrimary);
        std::string tag = (p.bank == 128 ? "drums " : "") + std::to_string(p.bank) + ":" + std::to_string(p.program);
        Text(tag, row.x + row.width - TextWidth(tag) - 8.0f, row.y + (row.height - kFontSize) / 2.0f, theme.textSecondary);
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) chosen = filtered[i];
    }
    if (filtered.empty()) Text("No presets match", list.x + 8.0f, list.y + 8.0f, theme.textSecondary);

    if (chosen >= 0) {
        if (pickRow >= 0 && (size_t)pickRow < rows.size()) {
            rows[(size_t)pickRow].preset = chosen;
            rows[(size_t)pickRow].score = 100;
            rows[(size_t)pickRow].include = true;
        } else if (pickRow == -2) {
            Row row;
            row.instrument = -1;
            row.preset = chosen;
            row.score = 100;
            row.include = true;
            rows.push_back(row);
        }
        closePicker = true;
    }
    if (closePicker) {
        picking = false;
        filter.clear();
        pickScroll = 0.0f;
    }
}

void Sf2ImportPanel::Render() {
    if (!session || !session->IsOpen()) return;
    const UIPalette& theme = UITheme::Get();
    const Rectangle r = bounds;
    DrawRectangleRec(r, theme.panelBg);
    ScissorStack::Push(r);

    float x = r.x + kPad;
    float w = r.width - kPad * 2.0f;
    float y = r.y + kPad;

    if (loaded) SyncRows();

    std::string title = loaded ? (soundFont.bankName.empty() ? fs::path(soundFontPath).filename().string() : soundFont.bankName) : std::string("No SoundFont loaded");
    Text(FitLeft(title, w - 130.0f), x, y + 5.0f, theme.textPrimary);
    if (Button({ x + w - 120.0f, y, 120.0f, 24.0f }, loaded ? "Change SoundFont..." : "Open SoundFont...", theme.accent)) {
        std::string defaultDir = loaded ? fs::path(soundFontPath).parent_path().string() : std::string();
        std::string picked = NativeFileDialog::OpenFile("Choose a SoundFont", defaultDir, "SoundFont (sf2)", "*.sf2");
        if (!picked.empty()) LoadSoundFont(picked);
    }
    y += 32.0f;

    if (loaded) {
        Text("Samples folder", x, y + 5.0f, theme.textSecondary);
        float labelWidth = 92.0f;
        Rectangle pathBox = { x + labelWidth, y, w - labelWidth - 164.0f, 24.0f };
        DrawRectangleRec(pathBox, theme.fieldBg);
        DrawRectangleLinesEx(pathBox, 1, theme.fieldBorder);
        Text(FitRight(destination, pathBox.width - 12.0f), pathBox.x + 6.0f, pathBox.y + 6.0f, destinationCustom ? theme.textPrimary : theme.textSecondary);
        if (Button({ pathBox.x + pathBox.width + 6.0f, y, 80.0f, 24.0f }, "Browse...", { 0, 0, 0, 0 })) {
            std::string picked = NativeFileDialog::OpenFolder("Choose where to put the samples", destination);
            if (!picked.empty()) { destination = picked; destinationCustom = true; }
        }
        if (Button({ pathBox.x + pathBox.width + 92.0f, y, 66.0f, 24.0f }, "Default", { 0, 0, 0, 0 }, destinationCustom)) ResetDestination();
        y += 34.0f;
    }

    if (!loaded) {
        Text("Open a .sf2 file to give your instruments sounds.", x, y + 8.0f, theme.textSecondary);
        if (!status.empty()) Text(FitLeft(status, w), x, y + 30.0f, statusIsError ? theme.danger : theme.textSecondary);
        ScissorStack::Pop();
        return;
    }

    const float footerHeight = 70.0f;
    Rectangle body = { x, y, w, r.y + r.height - footerHeight - y };

    if (picking) {
        DrawPicker(body);
    } else {
        Text("Import", body.x + 4.0f, body.y + 4.0f, theme.textSecondary);
        Text("Instrument", body.x + 52.0f, body.y + 4.0f, theme.textSecondary);
        Text("Preset", body.x + body.width * 0.42f, body.y + 4.0f, theme.textSecondary);
        DrawLineEx({ body.x, body.y + 20.0f }, { body.x + body.width, body.y + 20.0f }, 1.0f, theme.panelBorder);

        Rectangle list = { body.x, body.y + 22.0f, body.width, body.height - 22.0f };
        float contentHeight = (float)rows.size() * kRowHeight;
        float maxScroll = std::max(0.0f, contentHeight - list.height);
        if (Hover(list)) scroll -= GetMouseWheelMove() * 40.0f;
        scroll = std::clamp(scroll, 0.0f, maxScroll);

        const Sequence& seq = session->GetSequence();
        int removeRow = -1;
        for (size_t i = 0; i < rows.size(); i++) {
            Row& row = rows[i];
            Rectangle rr = { list.x, list.y + (float)i * kRowHeight - scroll, list.width, kRowHeight };
            if (rr.y + rr.height < list.y || rr.y > list.y + list.height) continue;
            bool inList = CheckCollisionPointRec(GetMousePosition(), list);
            if (i % 2 == 1) DrawRectangleRec(rr, Fade(BLACK, 0.12f));

            bool isNew = row.instrument < 0;
            std::string label;
            bool hasSamples = false;
            if (isNew) {
                label = "New instrument";
            } else if ((size_t)row.instrument < seq.instruments.size()) {
                const Instrument& inst = seq.instruments[(size_t)row.instrument];
                label = inst.name;
                for (const SampleSet& set : inst.sets) if (!set.zones.empty()) hasSamples = true;
            }

            Rectangle box = { rr.x + 4.0f, rr.y + 5.0f, 16.0f, 16.0f };
            if (isNew) {
                if (Button({ box.x, box.y, 16.0f, 16.0f }, "x", theme.danger) && inList) removeRow = (int)i;
            } else {
                bool hovered = Hover(box) && inList;
                DrawRectangleRec(box, theme.fieldBg);
                DrawRectangleLinesEx(box, 1, hovered ? theme.accent : theme.fieldBorder);
                if (row.include) DrawRectangleRec({ box.x + 4.0f, box.y + 4.0f, 8.0f, 8.0f }, theme.accent);
                if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && row.preset >= 0) row.include = !row.include;
            }

            float nameWidth = rr.width * 0.42f - 56.0f;
            Text(FitLeft(label + (hasSamples ? " (adds set)" : ""), nameWidth), rr.x + 52.0f, rr.y + (rr.height - kFontSize) / 2.0f, theme.textPrimary);

            Rectangle presetBtn = { rr.x + rr.width * 0.42f, rr.y + 2.0f, rr.width * 0.58f - 6.0f, rr.height - 4.0f };
            std::string presetText = PresetLabel(row.preset);
            Color fill = row.preset < 0 ? ColorBrightness(theme.buttonBg, -0.2f) : theme.buttonBg;
            bool clicked = Button(presetBtn, FitLeft(presetText, presetBtn.width - 70.0f), fill) && inList;
            if (row.preset >= 0 && row.score < 100) {
                std::string tag = row.score >= 85 ? "auto" : "guess";
                Text(tag, presetBtn.x + presetBtn.width - TextWidth(tag) - 6.0f, presetBtn.y + (presetBtn.height - kFontSize) / 2.0f, row.score >= 85 ? theme.success : theme.textSecondary);
            }
            if (clicked) {
                picking = true;
                pickRow = (int)i;
                filter.clear();
                pickScroll = 0.0f;
            }
        }
        if (removeRow >= 0) rows.erase(rows.begin() + removeRow);
        if (rows.empty()) Text("This sequence has no instruments yet.", list.x + 8.0f, list.y + 8.0f, theme.textSecondary);
    }

    float fy = r.y + r.height - footerHeight + 4.0f;
    DrawLineEx({ r.x, fy - 4.0f }, { r.x + r.width, fy - 4.0f }, 1.0f, theme.panelBorder);
    if (!status.empty()) Text(FitLeft(status, w), x, fy + 2.0f, statusIsError ? theme.danger : theme.textSecondary);
    float by = fy + 24.0f;
    if (!picking) {
        if (Button({ x, by, 190.0f, 28.0f }, "+ New Instrument from Preset", { 0, 0, 0, 0 })) {
            picking = true;
            pickRow = -2;
            filter.clear();
            pickScroll = 0.0f;
        }
        if (Button({ x + 198.0f, by, 100.0f, 28.0f }, "Auto-Match", { 0, 0, 0, 0 })) AutoMatchAll();
        if (Button({ x + w - 100.0f, by, 100.0f, 28.0f }, "Import", theme.accent)) RunImport();
    }

    ScissorStack::Pop();
}
