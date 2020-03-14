#include "hvc1.h"

#include <iostream>

#include "../codec.h"

#include "nal-slice.h"
#include "nal.h"

using namespace std;

int getSizeHvc1(Codec* self, const uchar* start, uint maxlength) {
	uint32_t length = 0;
	const uchar *pos = start;

	H265SliceInfo previous_slice;
	H265NalInfo previous_nal;
	self->was_keyframe_ = false;

	while(1) {
		logg(V, "---\n");
		H265NalInfo nal_info(pos, maxlength);
		if(!nal_info.is_ok){
			logg(V, "failed parsing h256 nal-header\n");
			return length;
		}

		switch(nal_info.nal_type_) {
		case NAL_AUD: // Access unit delimiter
			if (!previous_slice.is_ok)
				break;
			return length;
		case NAL_IDR_W_RADL:
		case NAL_IDR_N_LP:
			self->was_keyframe_ = true;
			[[fallthrough]];
		case NAL_TRAIL_N:
		case NAL_TRAIL_R:
		{
			H265SliceInfo slice_info(nal_info);
			if (!previous_slice.is_ok){
				previous_slice = slice_info;
				previous_nal = move(nal_info);
			}
			else {
				if (slice_info.isInNewFrame(previous_slice))
					return length;
				if (previous_nal.nuh_layer_id_ != nal_info.nuh_layer_id_){
					logg(W, "Different nuh_layer_id_ idc\n");
					return length;
				}
			}
			break;
		}
		case NAL_FILLER_DATA:
			if (g_log_mode >= V) {
				logg(V, "found filler data: ");
				printBuffer(pos, 30);
			}
			break;
		default:
			if(previous_slice.is_ok) {
				if(!nal_info.is_forbidden_set_) {
					logg(W2, "New access unit since seen picture (type: ", nal_info.nal_type_, ")\n");
					return length;
				} // otherwise it's malformed, don't produce an isolated malformed unit
			}
			break;
		}
		pos += nal_info.length_;
		length += nal_info.length_;
		maxlength -= nal_info.length_;
		if (maxlength == 0) // we made it
			return length;
		pos = self->loadAfter(length);
		logg(V, "Partial hvc1-length: ", length, "\n");
	}
	return length;
}
