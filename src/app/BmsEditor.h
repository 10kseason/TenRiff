#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace tenriff::app {
struct BmsEditorNote { std::uint64_t id=0; int lane=1,measure=0,slice=0,slice_count=192; bool long_note=false,editable=true; int end_measure=0,end_slice=0,end_slice_count=192; std::string channel,token,end_token; };
struct BmsEditorBgm { std::uint64_t id=0; int measure=0,slice=0,slice_count=192; bool editable=true; std::string token; };
struct BmsEditorMarker { enum class Kind { Bpm, Stop }; std::uint64_t id=0; int measure=0,slice=0,slice_count=1; Kind kind=Kind::Bpm; double value=0.0; std::string label,token; };
struct BmsEditorSample { std::string token,path; };
class BmsEditorDocument {
public:
 static constexpr int kGridDivision=192;
 bool load_file(const std::string&,std::string* error=nullptr); bool save_as(const std::string&,std::string* error=nullptr);
 bool loaded()const noexcept{return loaded_;} bool dirty()const noexcept{return dirty_;} const std::string& path()const noexcept{return path_;} const std::string& title()const noexcept{return title_;} const std::string& artist()const noexcept{return artist_;} const std::string& status()const noexcept{return status_;}
 int lane_count()const noexcept{return lane_count_;} int measure_count()const noexcept{return measure_count_;} int cursor_lane()const noexcept{return cursor_lane_;} int cursor_measure()const noexcept{return cursor_measure_;} int cursor_slice()const noexcept{return cursor_slice_;} int cursor_slice_count()const noexcept{return cursor_slice_count_;} double base_bpm()const noexcept{return base_bpm_;}
 std::size_t editable_note_count()const noexcept; std::size_t note_count()const noexcept{return notes_.size();} const std::vector<BmsEditorNote>& notes()const noexcept{return notes_;} const std::vector<BmsEditorBgm>& bgm()const noexcept{return bgm_;} const std::vector<BmsEditorMarker>& markers()const noexcept{return markers_;} const std::vector<BmsEditorSample>& samples()const noexcept{return samples_;} std::string selected_sample_token()const noexcept{return selected_sample_token_;} const BmsEditorNote* cursor_note()const noexcept;
 bool silent_note_mode()const noexcept{return silent_note_mode_;} bool toggle_silent_note_mode(); bool replace_silent_notes();
 void set_silent_note_mode(bool enabled) noexcept { silent_note_mode_ = enabled; }
 void set_cursor(int,int,int)noexcept; void set_cursor_exact(int,int,int,int)noexcept; void move_cursor(int,int,int)noexcept; bool select_note(std::uint64_t)noexcept;
 bool move_cursor_seconds(double) noexcept;
 std::optional<double> cursor_seconds() const;
 bool toggle_cursor_note(); bool remove_cursor_note(); bool add_cursor_hold(int,int); bool move_cursor_note(int,int,int); bool move_note(std::uint64_t,int,int,int,bool keep_time=false);
 bool move_bgm_to_cursor();
 void begin_note_drag(std::uint64_t id); void end_note_drag(); bool resize_cursor_note(int,int); bool move_bgm(std::uint64_t,int,int); bool move_bgm_selection(const std::vector<std::uint64_t>&,int,int); bool set_cursor_timing(BmsEditorMarker::Kind,double); bool remove_cursor_timing(BmsEditorMarker::Kind); bool set_base_bpm(double); bool select_sample(const std::string&); bool cycle_sample(int); std::string serialize_text(std::string* error=nullptr)const; bool undo(); bool redo();
private:
 struct State{std::vector<BmsEditorNote> notes;std::vector<BmsEditorBgm> bgm;std::vector<BmsEditorMarker> markers;double base_bpm;}; struct BmsChartStorage;
 void set_error(std::string,std::string*); void push_undo_state(); void sort_notes(); void sort_bgm(); std::string channel_for_lane(int)const; bool mutate_note(BmsEditorNote*); void refresh_dirty();
 std::uint64_t drag_id_=0; bool drag_undo_recorded_=false;
 bool loaded_=false,dirty_=false,silent_note_mode_=false; std::string path_,title_,artist_,status_; int lane_count_=10,measure_count_=1,cursor_lane_=1,cursor_measure_=0,cursor_slice_=0,cursor_slice_count_=kGridDivision; double base_bpm_=0.0; std::uint64_t next_id_=1,selected_id_=0; std::string selected_sample_token_; std::shared_ptr<BmsChartStorage> storage_; std::vector<BmsEditorNote> notes_; std::vector<BmsEditorBgm> bgm_; std::vector<BmsEditorMarker> markers_; std::vector<BmsEditorSample> samples_; std::vector<State> undo_stack_,redo_stack_; State saved_state_{}; std::string saved_text_;
}; }

