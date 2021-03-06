#include <caffe/caffe.hpp>
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <omp.h>
#include <boost/shared_ptr.hpp>
#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <time.h>

using namespace caffe;
using namespace std;
using namespace cv;

typedef struct{
	float x;
	float y;
	float w;
	float h;
	float score;
	int class_index;
} yolobox;

typedef struct
{
	int index;
	float probvalue;
}sortable_element;

typedef struct{
	int index;
	int classindex;
	float **probs;
} sortable_bbox;
class YOLODetect
{
public:
	YOLODetect();
	~YOLODetect();
public:
	YOLODetect(const string& proto_model_dir);
	std::vector<yolobox> YoloRegionDetect(const cv::Mat& testimage);
	void SetMean(const string& mean_file);
	void WrapInputLayer(std::vector<cv::Mat>* input_channels);
	void Preprocess(const cv::Mat& img, std::vector<cv::Mat>* input_channels);
	void Get_region_boxes();
	void do_nms_sort();
	void draw_detections();
	float overlap(float x1, float w1, float x2, float w2);
	void get_region_box(float *x, float *biases, int n, int index, int i, int j, int w, int h, yolobox &region_box);
	std::vector<yolobox> Do_NMS(std::vector<yolobox>& regboxes, float thresh, char methodtype);
	std::vector<yolobox> Do_NMSMultiLabel(std::vector<yolobox>& regboxes, std::vector<std::vector<float>>& probs, float thresh, float thresh_iou);
	float IoU(float xmin, float ymin, float xmax, float ymax, float xmin_, float ymin_, float xmax_, float ymax_, bool is_iom);
	int get_max_index(float *a, int n);
public:
	float Boxoverlap(float x1, float w1, float x2, float w2);
	float Calc_iou(const yolobox& box1, const yolobox& box2);
private:
	float *object_prob_;
	float *class_prob_;
	float *reg_box_;
	yolobox *reg_box_obj;
	sortable_bbox *sort_box_obj_;
	boost::shared_ptr<Net<float>> YoloNet_;
	cv::Size Input_geometry_;
	int num_channels_;
	cv::Mat mean_;
	float mean_value_[3];
	int num_objects_;
	int num_class_;
	int sidenum_;
	int locations_;
	clock_t time_start_;
	clock_t time_end_;
	bool sqrt_;
	bool constriant_;
	std::vector<yolobox> result_boxes;
	std::vector<yolobox> total_boxes_;
	std::vector<std::vector<float>> fprobs_;
};

YOLODetect::YOLODetect()
{

}

YOLODetect::~YOLODetect()
{

}

YOLODetect::YOLODetect(const string& proto_model_dir)
{
	Caffe::set_mode(Caffe::CPU);
	YoloNet_.reset(new Net<float>((proto_model_dir + "/yolodeploy.prototxt"), caffe::TEST));
	YoloNet_->CopyTrainedLayersFrom(proto_model_dir + "/yolov1_iter_good.caffemodel");

	//YoloNet_->CopyTrainedLayersFrom(proto_model_dir + "/fine_tuningresult.caffemodel");
	//YoloNet_->CopyTrainedLayersFrom(proto_model_dir + "/yolo.caffemodel");
	Blob<float>* input_layer = YoloNet_->input_blobs()[0];
	num_channels_ = input_layer->channels();
	Input_geometry_.width = input_layer->width();
	Input_geometry_.height = input_layer->height();
	num_objects_ = 2;
	num_class_ = 20;
	sidenum_ = 7;
	locations_ = pow(sidenum_,2);
	object_prob_ = (float *)malloc(num_objects_*locations_*sizeof(float));
	class_prob_ = (float *)malloc(num_class_*locations_*sizeof(float));
	reg_box_ = (float *)malloc(8 * locations_*sizeof(float));
	reg_box_obj = (yolobox *)malloc(num_objects_*locations_*sizeof(yolobox));
	sort_box_obj_ = (sortable_bbox *)malloc(num_objects_*locations_*sizeof(sortable_bbox));
	sqrt_ = true;
	constriant_ = true;
	result_boxes.resize(num_objects_*locations_);
	fprobs_.resize(num_objects_*locations_);
	for (int i = 0; i < fprobs_.size(); i++)
	{
		fprobs_[i].resize(num_class_);
	}
	mean_value_[0] = 104;
	mean_value_[1] = 117;
	mean_value_[2] = 123;
}

int YOLODetect::get_max_index(float *a, int n)
{
	if (n <= 0) return -1;
	int i, max_i = 0;
	float max = a[0];
	for (i = 1; i < n; ++i){
		if (a[i] > max){
			max = a[i];
			max_i = i;
		}
	}
	return max_i;
}

bool CompareBBox(const yolobox & a, const yolobox & b) {
	return a.score > b.score;
}
float YOLODetect::IoU(float xmin, float ymin, float xmax, float ymax,
	float xmin_, float ymin_, float xmax_, float ymax_, bool is_iom) {
	float iw = std::min(xmax, xmax_) - std::max(xmin, xmin_) + 1;
	float ih = std::min(ymax, ymax_) - std::max(ymin, ymin_) + 1;
	if (iw <= 0 || ih <= 0)
		return 0;
	float s = iw*ih;
	if (is_iom) {
		float ov = s / min((xmax - xmin + 1)*(ymax - ymin + 1), (xmax_ - xmin_ + 1)*(ymax_ - ymin_ + 1));
		return ov;
	}
	else {
		float ov = s / ((xmax - xmin + 1)*(ymax - ymin + 1) + (xmax_ - xmin_ + 1)*(ymax_ - ymin_ + 1) - s);
		return ov;
	}
}

std::vector<yolobox> YOLODetect::Do_NMS(std::vector<yolobox>& regboxes, float thresh, char methodtype)
{
	std::vector<yolobox> bboxes_nms;
	return bboxes_nms;
}

float YOLODetect::Boxoverlap(float x1, float w1, float x2, float w2)
{
	float left = std::max(x1 - w1 / 2, x2 - w2 / 2);
	float right = std::min(x1 + w1 / 2, x2 + w2 / 2);
	return right - left;
}

float YOLODetect::Calc_iou(const yolobox& box1, const yolobox& box2)
{
	float w = Boxoverlap(box1.x, box1.w, box2.x, box2.w);
	float h = Boxoverlap(box1.y, box1.h, box2.y, box2.h);
	if (w < 0 || h < 0) return 0;
	float inter_area = w * h;
	float union_area = box1.w * box1.h + box2.w * box2.h - inter_area;
	return inter_area / union_area;
}

bool CompareYoloBBox(const sortable_element & a, const sortable_element & b) {
	if (a.probvalue == b.probvalue)
		return a.index > b.index;
	else
		return a.probvalue > b.probvalue;
}

std::vector<yolobox> YOLODetect::Do_NMSMultiLabel(std::vector<yolobox>& regboxes, std::vector<std::vector<float>>& probs, float thresh, float threshiou)
{
	std::vector<yolobox> bboxes_nms;
	std::vector<sortable_element> sortelements;
	int totalnum = regboxes.size();
	assert(totalnum == probs.size());
	sortelements.resize(totalnum);
	//init
	for (int c = 0; c < num_class_; c++)
	{
		for (int k = 0; k < totalnum; k++)
		{
			sortelements[k].index = k;
			sortelements[k].probvalue = probs[k][c];
		}
		std::sort(sortelements.begin(), sortelements.end(), CompareYoloBBox);
		for (int i = 0; i < sortelements.size(); ++i)
		{
			if (probs[sortelements[i].index][c] < thresh)
			{
				probs[sortelements[i].index][c] = 0.0f;
				continue;
			}
			yolobox a = regboxes[sortelements[i].index];
			for (int j = i + 1; j < totalnum; ++j)
			{
				yolobox b = regboxes[sortelements[j].index];
				float calced_iou = Calc_iou(a, b);
				if (calced_iou > threshiou)
				{
					probs[sortelements[j].index][c] = 0.0f;
				}
			}
		}
	}
	for (int c = 0; c < num_class_; c++)
	{
		for (int k = 0; k < totalnum; k++)
		{
			if (probs[k][c] == 0.0f)
			    continue;
			regboxes[k].class_index = c;
			bboxes_nms.push_back(regboxes[k]);
		}
	}
	//float calced_iou111 = Calc_iou(bboxes_nms[0], bboxes_nms[1]);
	return bboxes_nms;
}

void YOLODetect::get_region_box(float *x, float *biases, int n, int index, int i, int j, int w, int h, yolobox &region_box)
{
	yolobox b;
	/*b.x = (i + logistic_activate(x[index + 0])) / w;
	b.y = (j + logistic_activate(x[index + 1])) / h;
	b.w = exp(x[index + 2]) * biases[2 * n];
	b.h = exp(x[index + 3]) * biases[2 * n + 1];
	if (DOABS){
		b.w = exp(x[index + 2]) * biases[2 * n] / w;
		b.h = exp(x[index + 3]) * biases[2 * n + 1] / h;
	}*/
}

void YOLODetect::Get_region_boxes()
{

}
void YOLODetect::do_nms_sort()
{

}
void YOLODetect::draw_detections()
{}

void YOLODetect::SetMean(const string& mean_file)
{
	BlobProto blob_proto;
	ReadProtoFromBinaryFileOrDie(mean_file.c_str(), &blob_proto);

	/* Convert from BlobProto to Blob<float> */
	Blob<float> mean_blob;
	mean_blob.FromProto(blob_proto);
	CHECK_EQ(mean_blob.channels(), num_channels_)
		<< "Number of channels of mean file doesn't match input layer.";

	/* The format of the mean file is planar 32-bit float BGR or grayscale. */
	std::vector<cv::Mat> channels;
	float* data = mean_blob.mutable_cpu_data();
	for (int i = 0; i < num_channels_; ++i) {
		/* Extract an individual channel. */
		cv::Mat channel(mean_blob.height(), mean_blob.width(), CV_32FC1, data);
		channels.push_back(channel);
		data += mean_blob.height() * mean_blob.width();
	}

	/* Merge the separate channels into a single image. */
	cv::Mat mean;
	cv::merge(channels, mean);

	/* Compute the global mean pixel value and create a mean image
	* filled with this value. */
	cv::Scalar channel_mean = cv::mean(mean);
	mean_ = cv::Mat(Input_geometry_, mean.type(), channel_mean);
}

/* Wrap the input layer of the network in separate cv::Mat objects
* (one per channel). This way we save one memcpy operation and we
* don't need to rely on cudaMemcpy2D. The last preprocessing
* operation will write the separate channels directly to the input
* layer. */
void YOLODetect::WrapInputLayer(std::vector<cv::Mat>* input_channels) 
{
	Blob<float>* input_layer = YoloNet_->input_blobs()[0];

	int width = input_layer->width();
	int height = input_layer->height();
	float* input_data = input_layer->mutable_cpu_data();
	for (int i = 0; i < input_layer->channels(); ++i) 
	{
		cv::Mat channel(height, width, CV_32FC1, input_data);
		input_channels->push_back(channel);
		input_data += width * height;
	}
}

void YOLODetect::Preprocess(const cv::Mat& img, std::vector<cv::Mat>* input_channels)
{
	/* Convert the input image to the input image format of the network. */
	cv::Mat sample;
	if (img.channels() == 3 && num_channels_ == 1)
		cv::cvtColor(img, sample, cv::COLOR_BGR2GRAY);
	else if (img.channels() == 4 && num_channels_ == 1)
		cv::cvtColor(img, sample, cv::COLOR_BGRA2GRAY);
	else if (img.channels() == 4 && num_channels_ == 3)
		cv::cvtColor(img, sample, cv::COLOR_BGRA2BGR);
	else if (img.channels() == 1 && num_channels_ == 3)
		cv::cvtColor(img, sample, cv::COLOR_GRAY2BGR);
	else
		sample = img;

	cv::Mat sample_resized;
	if (sample.size() != Input_geometry_)
		cv::resize(sample, sample_resized, Input_geometry_);
	else
		sample_resized = sample;

	cv::Mat sample_float;
	if (num_channels_ == 3)
		sample_resized.convertTo(sample_float, CV_32FC3);
	else
		sample_resized.convertTo(sample_float, CV_32FC1);

	cv::Mat sample_normalized;
	//cv::subtract(sample_float, mean_, sample_normalized);
	sample_float.copyTo(sample_normalized);

	/* This operation will write the separate BGR planes directly to the
	* input layer of the network because it is wrapped by the cv::Mat
	* objects in input_channels. */
	cv::split(sample_normalized, *input_channels);

	CHECK(reinterpret_cast<float*>(input_channels->at(0).data)
		== YoloNet_->input_blobs()[0]->cpu_data())
		<< "Input channels are not wrapping the input layer of the network.";
}

std::vector<yolobox> YOLODetect::YoloRegionDetect(const cv::Mat& testimage)
{
	int width = Input_geometry_.width;
	int height = Input_geometry_.height;
	Blob<float>* input_layer = YoloNet_->input_blobs()[0];
	input_layer->Reshape(1, num_channels_, Input_geometry_.height, Input_geometry_.width);
	YoloNet_->Reshape();
	std::vector<cv::Mat> input_channels;
	WrapInputLayer(&input_channels);
	Preprocess(testimage, &input_channels);
	/*float *input_data = input_layer->mutable_cpu_data();
	cv::Mat sample_resized;
	if (testimage.size() != Input_geometry_)
		cv::resize(testimage, sample_resized, Input_geometry_);
	else
		sample_resized = testimage;
	cv::Vec3b *img_data = (cv::Vec3b *)sample_resized.data;
	int spatial_size = width*height;
	for (int k = 0; k < spatial_size; ++k)
	{
		input_data[k] = float(img_data[k][0] - mean_value_[0]);
		input_data[k + spatial_size] = float(img_data[k][1] - mean_value_[1]);
		input_data[k + 2 * spatial_size] = float(img_data[k][2] - mean_value_[2]);
	}*/
	YoloNet_->Forward();

	Blob<float>* output_layer = YoloNet_->output_blobs()[0];
	float *output_data = output_layer->mutable_cpu_data();
	int blob_num = output_layer->num();
	int blob_insidecount = output_layer->count(1);
	for (int i = 0; i < output_layer->num(); ++i)
	{
		int index = i * blob_insidecount;
		for (int j = 0; j < locations_; ++j)
		{
			for (int k = 0; k < num_objects_; ++k)
			{
				int p_index = index + num_class_ * locations_ + k * locations_ + j;
				int array_index = k*locations_ + j; 
				object_prob_[array_index] = output_data[p_index];
			}

			for (int k = 0; k < num_class_; ++k)
			{
				int c_index = index + k*locations_ + j;
				int array_index = k*locations_ + j;
				int object1_index = j;
				int object2_index = locations_ + j;
				class_prob_[array_index] = output_data[c_index];
			}

			const float* box_pt = output_data + index + (num_class_ + num_objects_)*locations_ + j;
			for (int k = 0; k < num_objects_; ++k)
			{
				vector<float> box;
				box.push_back(*(box_pt + (k * 4 + 0) * locations_));
				box.push_back(*(box_pt + (k * 4 + 1) * locations_));
				box.push_back(*(box_pt + (k * 4 + 2) * locations_));
				box.push_back(*(box_pt + (k * 4 + 3) * locations_));
				if (constriant_) {
					box[0] = (j % sidenum_ + box[0]) / sidenum_;
					box[1] = (j / sidenum_ + box[1]) / sidenum_;
				}
				if (sqrt_) {
					box[2] = pow(box[2], 2);
					box[3] = pow(box[3], 2);
				}
				int array_index = k*locations_ + j;
				int array_ele_index = 4*(k*locations_ + j);
				reg_box_[array_ele_index] = box[0];
				reg_box_[array_ele_index+1] = box[1];
				reg_box_[array_ele_index+2] = box[2];
				reg_box_[array_ele_index+3] = box[3];
				reg_box_obj[array_index].x = box[0];
				reg_box_obj[array_index].y = box[1];
				reg_box_obj[array_index].w = box[2];
				reg_box_obj[array_index].h = box[3];
				reg_box_obj[array_index].score = object_prob_[array_index];
				result_boxes[array_index] = reg_box_obj[array_index];
				for (int c = 0; c < num_class_; c++)
				{
					fprobs_[array_index][c] = object_prob_[array_index] * class_prob_[c*locations_ + j];
				}
			}
		}
		std::vector<yolobox> bboxes_nms = Do_NMSMultiLabel(result_boxes, fprobs_, 0.15, 0.35);
		if (bboxes_nms.size() > 0) {
			total_boxes_.insert(total_boxes_.end(), bboxes_nms.begin(), bboxes_nms.end());
		}
	}
	return total_boxes_;
}


int main(int argc, char **argv)
{
	string root = "D:/Code_local/caffe/caffe_yolo";
	string modelroot = "D:/Code_local/caffe/caffe_yolo/test_models_self";
	string testimagefile = "D:/Code_local/caffe/caffe_yolo/testimage/test_yolo.jpg";
	//string testimagefile = "D:/Code_local/caffe/caffe_yolo/debug_result/test_yolo.jpg";
	cv::Mat testimage = imread(testimagefile);
	YOLODetect yolodetector(modelroot);
	double t = (double)cv::getTickCount();
	std::vector<yolobox> detected_box = yolodetector.YoloRegionDetect(testimage);
	std::cout << " time," << (double)(cv::getTickCount() - t) / cv::getTickFrequency() << "s" << std::endl;
	for (int i = 0; i < detected_box.size(); i++){
		int x = (int)((detected_box[i].x - detected_box[i].w/2)*testimage.cols);
		int y = (int)((detected_box[i].y - detected_box[i].h/2)*testimage.rows);
		int w = (int)((detected_box[i].w)*testimage.cols);
		int h = (int)((detected_box[i].h)*testimage.rows);
		cv::rectangle(testimage, cv::Rect(x, y, w, h), cv::Scalar(255, 0, 0), 2);
	}
	imwrite("D:/Code_local/caffe/caffe_yolo/debug_result/yolotestresult.jpg", testimage);
	FILE *fp = 0;
	fopen_s(&fp, "D:/Code_local/caffe/caffe_yolo/debug_result/box_info.txt", "w+");
	for (int i = 0; i < detected_box.size(); i++)
	{
		fprintf(fp, "%d  ", detected_box[i].class_index);
		fprintf(fp, "%f  ", detected_box[i].x);
		fprintf(fp, "%f  ", detected_box[i].y);
		fprintf(fp, "%f  ", detected_box[i].w);
		fprintf(fp, "%f  ", detected_box[i].h);
		fprintf(fp, "\n");
	}
	fclose(fp);
	return 1;
}
