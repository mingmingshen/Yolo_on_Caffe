#include <algorithm>
#include <cfloat>
#include <vector>
#include <cmath>

#include "caffe/layers/detection_loss_layer.hpp"
#include "caffe/layers/yolov3Detection_layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/util/bbox_util.hpp"

namespace caffe {
	template <typename Dtype>
	void Yolov3DetectLayer<Dtype>::LayerSetUp(
		const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
		Yolov3DetectParameter param = this->layer_param_.yolov3_detection_param();
		side_ = param.side();
		num_class_ = param.num_class();
		num_object_ = param.num();
		coords_ = param.coords();
		obj_threshold_ = param.obj_thresh();
		nms_threshold_ = param.nms_thresh();
		nms_ = param.nms();

		max_boxes_ = param.max_boxes();

		for (int c = 0; c < param.biases_size(); ++c) {
			biases_.push_back(param.biases(c));
		} //0.73 0.87;2.42 2.65;4.30 7.04;10.24 4.59;12.68 11.87;

		for (int m = 0; m < param.masks_size(); ++m) {
			//masks are different at different yolo layers
			masks_.push_back(param.masks(m));
		}

		int input_count = bottom[0]->count(1); //h*w*n*(classes+coords+1) = 13*13*5*(20+4+1), the size of the bottom input of region_loss_layer is h*w*filter_nums(filter_nums = 125)
		int label_count = bottom[1]->count(1); //30*5
		// outputs: classes, iou, coordinates
		int tmp_input_count = side_ * side_ * num_object_ * (coords_ + num_class_ + 1)*masks_.size()/num_object_; //13*13*5*(20+4+1) label: isobj, class_label, coordinates
		int tmp_label_count = max_boxes_ * (coords_ + 1);
		CHECK_EQ(input_count, tmp_input_count);//assertion checking
		CHECK_EQ(label_count, tmp_label_count);
	}

	template <typename Dtype>
	void Yolov3DetectLayer<Dtype>::Reshape(
		const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
		int input_count = bottom[0]->count(1);
		int label_count = bottom[1]->count(1);
		// outputs: classes, iou, coordinates
		int tmp_input_count = side_ * side_ * num_object_* (coords_ + num_class_ + 1);
		// label: isobj, class_label, coordinates
		int tmp_label_count = max_boxes_ * num_object_;
		CHECK_EQ(input_count, tmp_input_count);
		CHECK_EQ(label_count, tmp_label_count);

		vector<int> top_shape(2, 1);
		top_shape[0] = bottom[0]->num();
		top_shape[1] = side_ * side_ * (coords_ + 4);
		top[0]->Reshape(top_shape);
	}

	template <typename Dtype>
	void Yolov3DetectLayer<Dtype>::Forward_cpu(
		const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
		const Dtype* input_data = bottom[0]->cpu_data();
		const Dtype* label_data = bottom[1]->cpu_data();
		//*********************************************************Reshape********************************************************//
		Blob<Dtype> temp_bottom;
		temp_bottom.Reshape(bottom[0]->num(), bottom[0]->height()*bottom[0]->width(), num_object_, bottom[0]->channels() / num_object_);

		int basiclength = bottom[0]->channels() / num_object_;
		int label_basiclength = 5;

		Dtype *temp_bottomdata = temp_bottom.mutable_cpu_data();
		int temp_index = 0;
		for (int n = 0; n < bottom[0]->num(); n++)
		{
			for (int i = 0; i < bottom[0]->height(); i++)
			{
				for (int j = 0; j < bottom[0]->width(); j++)
				{
					for (int c = 0; c < bottom[0]->channels(); c++)
					{
						temp_bottomdata[temp_index++] = bottom[0]->data_at(n, c, i, j);
					}
				}
			}
		}
		Dtype* top_data = top[0]->mutable_cpu_data();
		caffe_set(top[0]->count(), Dtype(0), top_data);
		for (int i = 0; i < bottom[0]->num(); ++i) {
			int input_index = i * bottom[0]->count(1);
			int true_index = i * bottom[1]->count(1);
			int top_index = i * top[0]->count(1);
			map<int, vector<V3BoxData> > gt_boxes;
			GetYolov3GTBox(side_, label_basiclength, label_data + true_index, &gt_boxes);
			for (std::map<int, vector<V3BoxData > >::iterator it = gt_boxes.begin(); it != gt_boxes.end(); ++it) {
				int label = it->first;
				vector<V3BoxData>& g_boxes = it->second;
				for (int j = 0; j < g_boxes.size(); ++j) {
					top_data[top_index + label] += 1;
				}
			}
			map<int, vector<V3BoxData> > pred_boxes;
			//this step has finished nms already
			GetYolov3PredBoxes(side_, num_object_, num_class_, basiclength, temp_bottomdata + input_index, &pred_boxes, biases_, score_type_, nms_, obj_threshold_, nms_threshold_);
			int index = top_index + num_class_;
			int pred_count(0);
			for (std::map<int, vector<V3BoxData> >::iterator it = pred_boxes.begin(); it != pred_boxes.end(); ++it) {
				int label = it->first;
				vector<V3BoxData>& p_boxes = it->second;
				//如果预测的label不在groundtruth中，增加预测结果的个数
				if (gt_boxes.find(label) == gt_boxes.end()) {
					for (int b = 0; b < p_boxes.size(); ++b) {
						top_data[index + pred_count * 4 + 0] = p_boxes[b].label_;
						top_data[index + pred_count * 4 + 1] = p_boxes[b].score_;
						top_data[index + pred_count * 4 + 2] = 0;
						top_data[index + pred_count * 4 + 3] = 1;
						top_data[index + pred_count * 4 + 4] = p_boxes[b].box_[0];
						top_data[index + pred_count * 4 + 5] = p_boxes[b].box_[1];
						top_data[index + pred_count * 4 + 6] = p_boxes[b].box_[2];
						top_data[index + pred_count * 4 + 7] = p_boxes[b].box_[3];
						++pred_count;
					}
					continue;
				}
				//正常预测结果的统计
				vector<V3BoxData>& g_boxes = gt_boxes[label];
				vector<bool> records(g_boxes.size(), false);
				for (int k = 0; k < p_boxes.size(); ++k) {
					top_data[index + pred_count * 4 + 0] = p_boxes[k].label_;
					top_data[index + pred_count * 4 + 1] = p_boxes[k].score_;
					top_data[index + pred_count * 4 + 4] = p_boxes[k].box_[0];
					top_data[index + pred_count * 4 + 5] = p_boxes[k].box_[1];
					top_data[index + pred_count * 4 + 6] = p_boxes[k].box_[2];
					top_data[index + pred_count * 4 + 7] = p_boxes[k].box_[3];
					float max_iou(-1);
					int idx(-1);
					for (int g = 0; g < g_boxes.size(); ++g) {
						float iou = Calc_iou(p_boxes[k].box_, g_boxes[g].box_);
						if (iou > max_iou) {
							max_iou = iou;
							idx = g;
						}
					}
					if (max_iou >= obj_threshold_) {
						if (!records[idx]) {
							records[idx] = true;
							top_data[index + pred_count * 4 + 2] = 1;
							top_data[index + pred_count * 4 + 3] = 0;
						}
						else {
							top_data[index + pred_count * 4 + 2] = 0;
							top_data[index + pred_count * 4 + 3] = 1;
						}
					}
					else {
						top_data[index + pred_count * 4 + 2] = 0;
						top_data[index + pred_count * 4 + 3] = 1;
					}
					++pred_count;
				}
			}
		}
	}

	//INSTANTIATE_CLASS(yolov3DetectLayer);
	REGISTER_LAYER_CLASS(Yolov3Detect);

}  // namespace caffe
