#include "app/BmsEditor.h"
#include "chart/BmsParser.h"
#include "chart/BmsChartNorm.h"
#include "chart/BmsTimeline.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>
namespace tenriff::app {
namespace {
using chart::BmsChart; using chart::BmsMeasureCommand;
std::string upper(std::string s){for(char& c:s)if(c>='a'&&c<='z')c=char(c-'a'+'A');return s;}
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");auto b=s.find_last_not_of(" \t\r\n");return a==std::string::npos?std::string{}:s.substr(a,b-a+1);}
bool normal_channel(std::string_view c){return c.size()==2&&((c[0]=='1'||c[0]=='2')&&c[1]>='1'&&c[1]<='5');}
bool ln_channel(std::string_view c){return c.size()==2&&(c[0]=='5'||c[0]=='6')&&((c[1]>='1'&&c[1]<='6')||c[1]=='8'||c[1]=='9');}
std::string lane_channel(const BmsChart& c,int lane,bool ln){for(const auto& [ch,n]:c.lane_mapping.mapping())if(int(n)==lane&&((ln&&ln_channel(ch))||(!ln&&normal_channel(ch))))return ch;return{};}
bool nonzero(std::string_view s){return s.size()==2&&upper(std::string(s))!="00";}
std::string num(double v){std::ostringstream s;s<<std::setprecision(15)<<v;return s.str();}
std::string cmdline(int m,std::string ch,std::string d){std::ostringstream s;s<<"#"<<std::setw(3)<<std::setfill('0')<<m<<upper(ch)<<":"<<d;return s.str();}
int scaled(int x,int from,int to){if(from<=0)return 0; return int(std::llround(double(x)*to/double(from)));}
}
struct BmsEditorDocument::BmsChartStorage{BmsChart chart;};
void BmsEditorDocument::set_error(std::string m,std::string* e){status_=std::move(m);if(e)*e=status_;}
void BmsEditorDocument::refresh_dirty(){dirty_=loaded_&&serialize_text(nullptr)!=saved_text_;}
void BmsEditorDocument::sort_notes(){std::stable_sort(notes_.begin(),notes_.end(),[](const auto&a,const auto&b){if(a.measure!=b.measure)return a.measure<b.measure;double x=double(a.slice)/a.slice_count,y=double(b.slice)/b.slice_count;if(x!=y)return x<y;return a.lane<b.lane;});}
void BmsEditorDocument::sort_bgm(){std::stable_sort(bgm_.begin(),bgm_.end(),[](const auto&a,const auto&b){if(a.measure!=b.measure)return a.measure<b.measure;return double(a.slice)/std::max(1,a.slice_count)<double(b.slice)/std::max(1,b.slice_count);});}
void BmsEditorDocument::push_undo_state(){undo_stack_.push_back({notes_,bgm_,markers_,base_bpm_});if(undo_stack_.size()>128)undo_stack_.erase(undo_stack_.begin());redo_stack_.clear();}
std::size_t BmsEditorDocument::editable_note_count()const noexcept{return std::count_if(notes_.begin(),notes_.end(),[](const auto&n){return n.editable;});}
const BmsEditorNote* BmsEditorDocument::cursor_note()const noexcept{if(selected_id_)for(const auto&n:notes_)if(n.id==selected_id_)return &n;for(const auto&n:notes_)if(n.editable&&n.lane==cursor_lane_&&n.measure==cursor_measure_&&scaled(n.slice,n.slice_count,cursor_slice_count_)==cursor_slice_)return &n;return nullptr;}
std::string BmsEditorDocument::channel_for_lane(int l)const{return storage_?lane_channel(storage_->chart,l,false):std::string{};}
void BmsEditorDocument::set_cursor(int l,int m,int s)noexcept{set_cursor_exact(l,m,s,kGridDivision);}
void BmsEditorDocument::set_cursor_exact(int l,int m,int s,int d)noexcept{cursor_lane_=std::clamp(l,1,std::max(1,lane_count_));cursor_measure_=std::clamp(m,0,std::max(0,measure_count_-1));cursor_slice_count_=std::clamp(d,1,4096);cursor_slice_=std::clamp(s,0,cursor_slice_count_-1);selected_id_=0;}
void BmsEditorDocument::move_cursor(int l,int m,int s)noexcept{set_cursor_exact(cursor_lane_+l,cursor_measure_+m,cursor_slice_+s,cursor_slice_count_);}
bool BmsEditorDocument::move_cursor_seconds(double delta_seconds) noexcept {
 if(!loaded_||!storage_||!std::isfinite(delta_seconds))return false;
 chart::BmsChartNormalizer normalizer;auto normalized=normalizer.normalize(storage_->chart);if(!normalized.success())return false;
 const double current_position=static_cast<double>(cursor_measure_)+static_cast<double>(cursor_slice_)/std::max(1,cursor_slice_count_);
 chart::BmsNormalizedEvent marker;marker.type=chart::BmsNormalizedEventType::Unknown;marker.measure=cursor_measure_;marker.intra_measure=static_cast<double>(cursor_slice_)/std::max(1,cursor_slice_count_);marker.position=current_position;marker.object_id="__TENRIFF_EDITOR_SEEK_CURSOR__";normalized.chart.events.push_back(marker);std::stable_sort(normalized.chart.events.begin(),normalized.chart.events.end(),[](const auto&a,const auto&b){return a.position<b.position;});
 chart::BmsTimelineBuilder builder;auto timeline=builder.build(normalized.chart,1000);if(!timeline.success())return false;int64_t current_sample=0;for(const auto&e:timeline.timeline.events)if(e.event.object_id==marker.object_id){current_sample=e.time_samples;break;}const int64_t target_sample=std::clamp<int64_t>(current_sample+static_cast<int64_t>(std::llround(delta_seconds*1000.0)),0,std::max<int64_t>(0,timeline.timeline.duration_samples));double target_position=current_position;if(!timeline.timeline.scroll_segments.empty()){for(const auto&segment:timeline.timeline.scroll_segments){if(target_sample<segment.start_sample)break;if(target_sample<=segment.end_sample){const double span=static_cast<double>(segment.end_sample-segment.start_sample);target_position=span>0.0?segment.start_position+(segment.end_position-segment.start_position)*std::clamp((target_sample-segment.start_sample)/span,0.0,1.0):segment.start_position;break;}target_position=segment.end_position;}}else if(!normalized.chart.measures.empty())target_position=normalized.chart.measures.back().end();const int grid=kGridDivision;int measure=std::max(0,static_cast<int>(std::floor(target_position+1e-9)));int slice=static_cast<int>(std::llround((target_position-measure)*grid));if(slice>=grid){++measure;slice=0;}set_cursor_exact(cursor_lane_,measure,slice,grid);return true;
}
std::optional<double> BmsEditorDocument::cursor_seconds() const {
 if(!loaded_||!storage_)return std::nullopt;
 chart::BmsChartNormalizer normalizer;auto normalized=normalizer.normalize(storage_->chart);if(!normalized.success())return std::nullopt;
 chart::BmsNormalizedEvent marker;marker.type=chart::BmsNormalizedEventType::Unknown;marker.measure=cursor_measure_;
 marker.intra_measure=static_cast<double>(cursor_slice_)/std::max(1,cursor_slice_count_);
 marker.position=static_cast<double>(cursor_measure_)+marker.intra_measure;marker.object_id="__TENRIFF_EDITOR_CURSOR_SECONDS__";
 normalized.chart.events.push_back(marker);std::stable_sort(normalized.chart.events.begin(),normalized.chart.events.end(),[](const auto&a,const auto&b){return a.position<b.position;});
 chart::BmsTimelineBuilder builder;auto timeline=builder.build(normalized.chart,1000);if(!timeline.success())return std::nullopt;
 for(const auto&e:timeline.timeline.events)if(e.event.object_id==marker.object_id)return static_cast<double>(e.time_samples)/1000.0;
 return std::nullopt;
}
bool BmsEditorDocument::select_note(std::uint64_t id)noexcept{for(const auto&n:notes_)if(n.id==id){selected_id_=id;set_cursor_exact(n.lane,n.measure,n.slice,n.slice_count);selected_id_=id;return true;}return false;}
bool BmsEditorDocument::mutate_note(BmsEditorNote*n){
 if(!n||!n->editable){status_="Note is read-only";return false;}
 if(drag_id_!=n->id||!drag_undo_recorded_){push_undo_state();if(drag_id_==n->id)drag_undo_recorded_=true;}
 dirty_=true;return true;
}
void BmsEditorDocument::begin_note_drag(std::uint64_t id){drag_id_=id;drag_undo_recorded_=false;}
void BmsEditorDocument::end_note_drag(){drag_id_=0;drag_undo_recorded_=false;}
bool BmsEditorDocument::toggle_silent_note_mode(){silent_note_mode_=!silent_note_mode_;status_=silent_note_mode_?"Silent note mode":"Keysound note mode";return true;}
bool BmsEditorDocument::replace_silent_notes(){if(!loaded_||selected_sample_token_.empty())return false;std::size_t count=0;for(const auto&n:notes_)if(n.editable&&(n.token.empty()||upper(n.token)=="00"))++count;if(count==0){status_="No silent notes";return false;}push_undo_state();for(auto&n:notes_)if(n.editable&&(n.token.empty()||upper(n.token)=="00")){n.token=selected_sample_token_;if(n.long_note&&n.end_token.empty())n.end_token=selected_sample_token_;}dirty_=true;status_="Silent notes converted to "+selected_sample_token_;sort_notes();return true;}
bool BmsEditorDocument::toggle_cursor_note(){if(!loaded_)return false;if(auto*n=const_cast<BmsEditorNote*>(cursor_note())){if(!mutate_note(n))return false;notes_.erase(std::remove_if(notes_.begin(),notes_.end(),[&](const auto&x){return x.id==n->id;}),notes_.end());selected_id_=0;status_="Note removed";return true;}auto ch=channel_for_lane(cursor_lane_);if(ch.empty()){status_="No editable lane";return false;}push_undo_state();const std::string token=silent_note_mode_?"00":selected_sample_token_;notes_.push_back({next_id_++,cursor_lane_,cursor_measure_,cursor_slice_,cursor_slice_count_,false,true,cursor_measure_,cursor_slice_,cursor_slice_count_,ch,token,{}});dirty_=true;status_=silent_note_mode_?"Silent note added":"Note added";sort_notes();return true;}
bool BmsEditorDocument::remove_cursor_note(){if(auto*n=const_cast<BmsEditorNote*>(cursor_note())){if(!mutate_note(n))return false;const auto id=n->id;notes_.erase(std::remove_if(notes_.begin(),notes_.end(),[&](const auto&x){return x.id==id;}),notes_.end());selected_id_=0;status_="Note removed";return true;}status_="No note at cursor";return false;}
bool BmsEditorDocument::add_cursor_hold(int em,int es){if(!loaded_)return false;if(auto*n=const_cast<BmsEditorNote*>(cursor_note())){es=std::clamp(es,0,kGridDivision-1);em=std::max(0,em);if(em<n->measure||(em==n->measure&&es<=scaled(n->slice,n->slice_count,kGridDivision))){status_="Hold end must be after its start";return false;}if(!mutate_note(n))return false;n->long_note=true;n->end_measure=em;n->end_slice=es;n->end_slice_count=kGridDivision;n->end_token=n->token;dirty_=true;status_="Hold added";return true;}return false;}
bool BmsEditorDocument::move_cursor_note(int l,int m,int s){
 const auto*n=cursor_note();return n&&move_note(n->id,l,m,s);
}
bool BmsEditorDocument::move_note(std::uint64_t id,int l,int m,int s,bool keep_time){
 auto it=std::find_if(notes_.begin(),notes_.end(),[&](const auto&n){return n.id==id;});
 if(it==notes_.end()||!it->editable)return false;
 l=std::clamp(l,1,lane_count_);m=std::max(0,m);s=std::clamp(s,0,kGridDivision-1);
 int d=kGridDivision;
 if(keep_time){m=it->measure;s=it->slice;d=it->slice_count;}
 const double old_start=it->measure+double(it->slice)/it->slice_count;
 const double new_start=m+double(s)/d;
 const double delta=new_start-old_start;
 if(l==it->lane&&std::abs(delta)<1e-10)return false;
 const std::string channel=channel_for_lane(l);if(channel.empty())return false;
 // Refuse an overlapping destination: serialization must never discard a note.
 const double new_end=it->long_note?it->end_measure+double(it->end_slice)/it->end_slice_count+delta:new_start;
 for(const auto&other:notes_){
  if(other.id==id||other.lane!=l)continue;
  const double start=other.measure+double(other.slice)/other.slice_count;
  const double end=other.long_note?other.end_measure+double(other.end_slice)/other.end_slice_count:start;
  if(new_start<=end+1e-10&&new_end>=start-1e-10){status_="Destination already contains a note";return false;}
 }
 // Shift both ends on a common exact grid; a horizontal-only move keeps the
 // original fractions, including timing that is not divisible by 192.
 int end_d=it->end_slice_count,end_m=it->end_measure,end_s=it->end_slice;
 if(it->long_note&&std::abs(delta)>1e-10){
  const auto common=std::lcm(std::lcm(static_cast<std::int64_t>(d),static_cast<std::int64_t>(it->slice_count)),static_cast<std::int64_t>(end_d));
  if(common>65536){status_="Cannot preserve this hold fraction on the destination grid";return false;}
  const int64_t end=static_cast<int64_t>(end_m)*common+end_s*(common/end_d)+
   static_cast<int64_t>(m-it->measure)*common+s*(common/d)-it->slice*(common/it->slice_count);
  end_d=static_cast<int>(common);end_m=static_cast<int>(end/common);end_s=static_cast<int>(end%common);
 }
 if(!mutate_note(&*it))return false;
 if(std::abs(delta)>1e-10){it->measure=m;it->slice=s;it->slice_count=d;}
 it->lane=l;it->channel=channel;
 if(it->long_note){it->end_measure=end_m;it->end_slice=end_s;it->end_slice_count=end_d;}
 measure_count_=std::max(measure_count_,std::max(it->measure,it->end_measure)+1);
 sort_notes();select_note(id);status_="Note moved";return true;
}
bool BmsEditorDocument::move_bgm_to_cursor(){
 if(!loaded_)return false;
 auto match=[&](const auto&b){return b.editable&&b.measure==cursor_measure_&&
  static_cast<int64_t>(b.slice)*cursor_slice_count_==static_cast<int64_t>(cursor_slice_)*b.slice_count;};
 auto bit=std::find_if(bgm_.begin(),bgm_.end(),[&](const auto&b){return match(b)&&b.token==selected_sample_token_;});
 if(bit==bgm_.end())bit=std::find_if(bgm_.begin(),bgm_.end(),match);
 if(bit==bgm_.end())return false;
 const auto bgm=*bit;const std::string channel=channel_for_lane(cursor_lane_);if(channel.empty())return false;
 auto*n=const_cast<BmsEditorNote*>(cursor_note());if(n&&!n->editable)return false;
 if(n&&n->long_note){status_="Select a tap or empty position for BGM transfer";return false;}
 push_undo_state();
 // Swap an existing tap back into BGM so all sound events keep their timing.
 if(n){const auto old=n->token;n->token=bgm.token;selected_id_=n->id;
  if(nonzero(old)){bit->token=old;}else{bgm_.erase(bit);}
 }else{
  const auto id=next_id_++;
  notes_.push_back({id,cursor_lane_,bgm.measure,bgm.slice,bgm.slice_count,false,true,
                    bgm.measure,bgm.slice,bgm.slice_count,channel,bgm.token,{}});
  bgm_.erase(bit);sort_notes();selected_id_=id;
 }
 dirty_=true;status_="BGM moved to lane";return true;
}
bool BmsEditorDocument::resize_cursor_note(int em,int es){auto*n=const_cast<BmsEditorNote*>(cursor_note());if(!n||!n->long_note)return false;es=std::clamp(es,0,kGridDivision-1);em=std::max(0,em);if(em<n->measure||(em==n->measure&&es<=scaled(n->slice,n->slice_count,kGridDivision))){status_="Hold end must be after its start";return false;}if(!mutate_note(n))return false;n->end_measure=em;n->end_slice=es;n->end_slice_count=kGridDivision;dirty_=true;return true;}
bool BmsEditorDocument::move_bgm(std::uint64_t id,int m,int s){std::vector<std::uint64_t> ids{id};return move_bgm_selection(ids,m,s);}
bool BmsEditorDocument::move_bgm_selection(const std::vector<std::uint64_t>& ids,int dm,int ds){if(!loaded_||ids.empty())return false;std::set<std::uint64_t> wanted(ids.begin(),ids.end());bool found=false;for(const auto&b:bgm_)if(wanted.count(b.id))found=true;if(!found)return false;push_undo_state();for(auto&b:bgm_)if(wanted.count(b.id)){int absolute=std::max(0,b.measure*kGridDivision+scaled(b.slice,b.slice_count,kGridDivision)+kGridDivision*dm+ds);b.measure=absolute/kGridDivision;b.slice=absolute%kGridDivision;b.slice_count=kGridDivision;}sort_bgm();dirty_=true;status_="BGM position moved";return true;}
bool BmsEditorDocument::set_cursor_timing(BmsEditorMarker::Kind k,double v){if(!loaded_||!std::isfinite(v)||v<=0)return false;push_undo_state();markers_.erase(std::remove_if(markers_.begin(),markers_.end(),[&](const auto&m){return m.kind==k&&m.measure==cursor_measure_&&scaled(m.slice,m.slice_count,cursor_slice_count_)==cursor_slice_;}),markers_.end());markers_.push_back({next_id_++,cursor_measure_,cursor_slice_,cursor_slice_count_,k,v,(k==BmsEditorMarker::Kind::Stop?"STOP ":"BPM ")+num(v),{}});dirty_=true;return true;}
bool BmsEditorDocument::remove_cursor_timing(BmsEditorMarker::Kind k){auto it=std::find_if(markers_.begin(),markers_.end(),[&](const auto&m){return m.kind==k&&m.measure==cursor_measure_&&scaled(m.slice,m.slice_count,cursor_slice_count_)==cursor_slice_;});if(it==markers_.end())return false;push_undo_state();markers_.erase(it);dirty_=true;return true;}
bool BmsEditorDocument::set_base_bpm(double v){if(!std::isfinite(v)||v<=0)return false;push_undo_state();base_bpm_=v;dirty_=true;return true;}
bool BmsEditorDocument::select_sample(const std::string&t){for(const auto&s:samples_)if(upper(s.token)==upper(t)){selected_sample_token_=upper(t);return true;}return false;}
bool BmsEditorDocument::cycle_sample(int d){if(samples_.empty())return false;auto it=std::find_if(samples_.begin(),samples_.end(),[&](const auto&s){return upper(s.token)==upper(selected_sample_token_);});int i=it==samples_.end()?0:int(it-samples_.begin());i=(i+d%int(samples_.size())+int(samples_.size()))%int(samples_.size());selected_sample_token_=samples_[i].token;return true;}
bool BmsEditorDocument::load_file(const std::string&p,std::string*err){
 std::ifstream raw;try{raw.open(std::filesystem::u8path(p),std::ios::binary);}catch(...){raw.open(p,std::ios::binary);}std::ostringstream rb;rb<<raw.rdbuf();auto bytes=rb.str();auto up=upper(bytes);if(up.find("#RANDOM")!=std::string::npos||up.find("#SWITCH")!=std::string::npos){set_error("Conditional RANDOM/SWITCH charts are not editable.",err);return false;}if(up.find("#LNTYPE:2")!=std::string::npos){set_error("LNTYPE 2 charts are not editable by this MVP.",err);return false;}
 auto lnpos=up.find("#LNTYPE");if(lnpos!=std::string::npos){auto end=up.find('\n',lnpos);auto tail=trim(up.substr(lnpos+7,end==std::string::npos?std::string::npos:end-lnpos-7));if(!tail.empty()&&tail.front()==':')tail=trim(tail.substr(1));if(!tail.empty()&&tail.front()=='2'){set_error("LNTYPE 2 charts are not editable by this MVP.",err);return false;}} chart::BmsParser parser;chart::BmsParserOptions o;o.tolerant=false;o.retain_nonessential_commands=true;o.retain_unknown_headers=true;auto r=parser.parseFile(p,o);if(!r.success()){for(const auto&m:r.messages)if(m.severity==chart::BmsParseSeverity::Error){set_error(m.text,err);return false;}}
 storage_=std::make_shared<BmsChartStorage>();storage_->chart=r.chart;path_=p;title_=r.chart.headers["TITLE"];artist_=r.chart.headers["ARTIST"];base_bpm_=r.chart.base_bpm;lane_count_=std::clamp(r.chart.declared_key_count>0?r.chart.declared_key_count:10,1,16);measure_count_=std::max(1,r.chart.last_measure_index+1);for(const auto&c:r.chart.commands)measure_count_=std::max(measure_count_,c.measure+1);
 samples_.clear();for(const auto&[t,v]:r.chart.wav)samples_.push_back({upper(t),v});std::sort(samples_.begin(),samples_.end(),[](auto&a,auto&b){return a.token<b.token;});if(!samples_.empty())selected_sample_token_=samples_[0].token;
 notes_.clear();bgm_.clear();markers_.clear();next_id_=1;std::unordered_map<int,std::size_t> pending;std::unordered_map<int,std::size_t> last;std::string lnobj=upper(trim(r.chart.headers["LNOBJ"]));std::set<std::size_t> skip;
 for(const auto&c:r.chart.commands){if(c.data.empty())continue;int n=int(c.data.size()/2);if(c.channel=="01"){for(int i=0;i<n;i++){auto t=upper(c.data.substr(i*2,2));if(nonzero(t))bgm_.push_back({next_id_++,c.measure,i,n,true,t});}continue;}if(c.channel=="03"||c.channel=="08"||c.channel=="09"){for(int i=0;i<n;i++){auto t=upper(c.data.substr(i*2,2));if(!nonzero(t))continue;double v=0;bool ok=true;if(c.channel=="03"){try{v=std::stoi(t,nullptr,16);}catch(...){ok=false;}}else if(c.channel=="08"){auto it=r.chart.bpm.find(t);ok=it!=r.chart.bpm.end();if(ok)v=it->second;}else{auto it=r.chart.stop.find(t);ok=it!=r.chart.stop.end();if(ok)v=it->second;}if(ok){auto k=c.channel=="09"?BmsEditorMarker::Kind::Stop:BmsEditorMarker::Kind::Bpm;markers_.push_back({next_id_++,c.measure,i,n,k,v,(k==BmsEditorMarker::Kind::Stop?"STOP ":"BPM ")+num(v),t});}}continue;}if(!normal_channel(c.channel)&&!ln_channel(c.channel))continue;auto lane=r.chart.lane_mapping.laneForChannel(c.channel);if(!lane)continue;for(int i=0;i<n;i++){auto t=upper(c.data.substr(i*2,2));if(!nonzero(t))continue;int l=int(*lane);if(!ln_channel(c.channel)&&!lnobj.empty()&&t==lnobj&&last.count(l)){auto&x=notes_[last[l]];if(!x.long_note&&!(x.measure==c.measure&&x.slice==i)){x.long_note=true;x.end_measure=c.measure;x.end_slice=i;x.end_slice_count=n;x.end_token=t;}last.erase(l);continue;}if(ln_channel(c.channel)){auto pi=pending.find(l);if(pi==pending.end()){notes_.push_back({next_id_++,l,c.measure,i,n,true,true,c.measure,i,n,c.channel,t,{}});pending[l]=notes_.size()-1;}else{auto&x=notes_[pi->second];if(c.measure>x.measure||i*x.slice_count>x.slice*n){x.end_measure=c.measure;x.end_slice=i;x.end_slice_count=n;x.end_token=t;}else{x.long_note=false;}pending.erase(pi);}continue;}notes_.push_back({next_id_++,l,c.measure,i,n,false,true,c.measure,i,n,c.channel,t,{}});last[l]=notes_.size()-1;}}
 sort_notes();sort_bgm();for(std::size_t i=1;i<notes_.size();++i){const auto&a=notes_[i-1];const auto&b=notes_[i];if(a.lane==b.lane&&a.measure==b.measure&&a.slice*b.slice_count==b.slice*a.slice_count&&a.token!=b.token){set_error("Ambiguous duplicate notes at the same lane and timing.",err);return false;}}cursor_lane_=1;cursor_measure_=0;cursor_slice_=0;cursor_slice_count_=192;selected_id_=0;silent_note_mode_=false;undo_stack_.clear();redo_stack_.clear();saved_state_={notes_,bgm_,markers_,base_bpm_};loaded_=true;saved_text_=serialize_text(nullptr);dirty_=false;status_="Loaded";return true;
}
 std::string BmsEditorDocument::serialize_text(std::string*err)const{
 if(!loaded_||!storage_){if(err)*err="No BMS chart is loaded.";return{};}const auto&c=storage_->chart;std::ostringstream o;std::vector<std::string> keys;for(auto&[k,v]:c.headers){(void)v;keys.push_back(k);}std::sort(keys.begin(),keys.end());for(auto&k:keys){if(k=="BPM")o<<"#BPM:"<<num(base_bpm_)<<"\n";else o<<"#"<<k<<":"<<c.headers.at(k)<<"\n";}auto emit_text=[&](std::string pre,const auto&map){std::vector<std::pair<std::string,std::string>> x;for(auto&[k,v]:map)x.emplace_back(upper(k),v);std::sort(x.begin(),x.end());for(auto&[k,v]:x)o<<"#"<<pre<<k<<":"<<v<<"\n";};auto emit_num=[&](std::string pre,const auto&map){std::vector<std::pair<std::string,double>> x;for(auto&[k,v]:map)x.emplace_back(upper(k),v);std::sort(x.begin(),x.end());for(auto&[k,v]:x)o<<"#"<<pre<<k<<":"<<num(v)<<"\n";};emit_text("WAV",c.wav);emit_text("BMP",c.bmp);emit_num("BPM",c.bpm);emit_num("STOP",c.stop);emit_num("SCROLL",c.scroll);
 std::set<std::string> replaced;for(auto&cmd:c.commands)if(normal_channel(cmd.channel)||ln_channel(cmd.channel)||cmd.channel=="01"||cmd.channel=="03"||cmd.channel=="08"||cmd.channel=="09")replaced.insert(cmd.channel);std::vector<BmsMeasureCommand> cmds;for(auto&cmd:c.commands)if(!replaced.count(cmd.channel))cmds.push_back(cmd);
 const std::string lnobj=upper(c.headers.count("LNOBJ")?c.headers.at("LNOBJ"):std::string{});std::map<std::tuple<int,std::string,int>,std::string> data;for(const auto&n:notes_){auto ch=n.channel;if(ch.empty())ch=channel_for_lane(n.lane);if(n.long_note&&normal_channel(ch)&&upper(n.end_token)!=lnobj){ch[0]=ch[0]=='1'?'5':'6';}if(ch.empty())continue;auto&d=data[{n.measure,ch,n.slice_count}];if(d.empty())d.assign(n.slice_count*2,'0');if(n.slice>=0&&n.slice<n.slice_count)d.replace(n.slice*2,2,n.token.empty()?"01":n.token);if(n.long_note){auto ec=ch;auto&ed=data[{n.end_measure,ec,n.end_slice_count}];if(ed.empty())ed.assign(n.end_slice_count*2,'0');if(n.end_slice>=0&&n.end_slice<n.end_slice_count)ed.replace(n.end_slice*2,2,n.end_token.empty()?(n.token.empty()?"01":n.token):n.end_token);}}
 for(const auto&b:bgm_){std::string d(b.slice_count*2,'0');if(b.slice>=0&&b.slice<b.slice_count)d.replace(b.slice*2,2,b.token.empty()?"01":b.token);cmds.push_back({b.measure,"01",std::move(d)});}

 std::set<std::string> generated_bpm,generated_stop;for(const auto&m:markers_){std::string ch=m.kind==BmsEditorMarker::Kind::Stop?"09":"03";std::string tok=m.token;if(tok.empty()){if(m.kind==BmsEditorMarker::Kind::Bpm){int iv=int(std::llround(m.value));if(iv>0&&iv<=255){std::ostringstream s;s<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<iv;tok=s.str();}else{tok="E0";while(c.bpm.count(tok)||generated_bpm.count(tok))tok[0]++;generated_bpm.insert(tok);o<<"#BPM"<<tok<<":"<<num(m.value)<<"\n";ch="08";}}else{tok="E0";while(c.stop.count(tok)||generated_stop.count(tok))tok[0]++;generated_stop.insert(tok);o<<"#STOP"<<tok<<":"<<num(m.value)<<"\n";}}auto&d=data[{m.measure,ch,m.slice_count}];if(d.empty())d.assign(m.slice_count*2,'0');d.replace(m.slice*2,2,tok);}
 for(auto&[k,d]:data)cmds.push_back({std::get<0>(k),std::get<1>(k),d});std::stable_sort(cmds.begin(),cmds.end(),[](auto&a,auto&b){return a.measure==b.measure?a.channel<b.channel:a.measure<b.measure;});for(auto&x:cmds)o<<cmdline(x.measure,x.channel,x.data)<<"\n";return o.str();
}
bool BmsEditorDocument::save_as(const std::string&p,std::string*err){if(!loaded_){set_error("No BMS chart is loaded.",err);return false;}std::error_code ec;auto out=std::filesystem::u8path(p),in=std::filesystem::u8path(path_);if(p.empty()||std::filesystem::exists(out,ec)){set_error("Save As refuses to overwrite an existing file.",err);return false;}if(std::filesystem::equivalent(out.parent_path(),in.parent_path(),ec)==false&&out.parent_path()!=in.parent_path()){set_error("Save As must stay beside the source chart so media references remain valid.",err);return false;}std::string local_error;auto text=serialize_text(&local_error);if(text.empty()){set_error(local_error.empty()?"Serialization produced no output.":local_error,err);return false;}std::ofstream f(out,std::ios::binary|std::ios::out|std::ios::trunc);if(!f){set_error("Failed to open save path.",err);return false;}f.write(text.data(),text.size());if(!f){set_error("Failed to write save path.",err);return false;}path_=p;saved_text_=text;dirty_=false;saved_state_={notes_,bgm_,markers_,base_bpm_};status_="Saved";return true;}
bool BmsEditorDocument::undo(){if(undo_stack_.empty())return false;redo_stack_.push_back({notes_,bgm_,markers_,base_bpm_});auto s=std::move(undo_stack_.back());undo_stack_.pop_back();notes_=std::move(s.notes);bgm_=std::move(s.bgm);markers_=std::move(s.markers);base_bpm_=s.base_bpm;refresh_dirty();status_="Undo";return true;}
bool BmsEditorDocument::redo(){if(redo_stack_.empty())return false;undo_stack_.push_back({notes_,bgm_,markers_,base_bpm_});auto s=std::move(redo_stack_.back());redo_stack_.pop_back();notes_=std::move(s.notes);bgm_=std::move(s.bgm);markers_=std::move(s.markers);base_bpm_=s.base_bpm;refresh_dirty();status_="Redo";return true;}
}

