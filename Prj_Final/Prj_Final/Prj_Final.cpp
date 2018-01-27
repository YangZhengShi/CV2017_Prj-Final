// Prj_Final.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include <iostream>  
#include <string>  
#include "opencv2/highgui/highgui.hpp"  
#include <opencv2/imgproc/imgproc.hpp> 
#include "opencv2/core/core.hpp"  
#include<stdlib.h>
#include <opencv2/stitching/stitcher.hpp>
#include"opencv2/nonfree/nonfree.hpp"
#include"opencv2/legacy/legacy.hpp"

using namespace std;
using namespace cv;
typedef struct
{
	Point2f left_top;
	Point2f left_bottom;
	Point2f right_top;
	Point2f right_bottom;
}four_corners_t;

void OptimizeSeam(Mat &img1, Mat &trans, Mat& dst);

four_corners_t corners;
string IMAGE_PATH_PREFIX = "./image/";
bool try_use_gpu = false;
vector<Mat> imgs;
string result_name = IMAGE_PATH_PREFIX + "result.jpg";

//����ͼƬ�ĸ��ǵ�ֵ
void CalcCorners(const Mat& H, const Mat& src)
{
	double v2[] = { 0, 0, 1 };//���Ͻ�
	double v1[3];//�任�������ֵ
	Mat V2 = Mat(3, 1, CV_64FC1, v2);  //������
	Mat V1 = Mat(3, 1, CV_64FC1, v1);  //������

	V1 = H * V2;
	//���Ͻ�(0,0,1)
	cout << "V2: " << V2 << endl;
	cout << "V1: " << V1 << endl;
	corners.left_top.x = v1[0] / v1[2];
	corners.left_top.y = v1[1] / v1[2];

	//���½�(0,src.rows,1)
	v2[0] = 0;
	v2[1] = src.rows;
	v2[2] = 1;
	V2 = Mat(3, 1, CV_64FC1, v2);  //������
	V1 = Mat(3, 1, CV_64FC1, v1);  //������
	V1 = H * V2;
	corners.left_bottom.x = v1[0] / v1[2];
	corners.left_bottom.y = v1[1] / v1[2];

	//���Ͻ�(src.cols,0,1)
	v2[0] = src.cols;
	v2[1] = 0;
	v2[2] = 1;
	V2 = Mat(3, 1, CV_64FC1, v2);  //������
	V1 = Mat(3, 1, CV_64FC1, v1);  //������
	V1 = H * V2;
	corners.right_top.x = v1[0] / v1[2];
	corners.right_top.y = v1[1] / v1[2];

	//���½�(src.cols,src.rows,1)
	v2[0] = src.cols;
	v2[1] = src.rows;
	v2[2] = 1;
	V2 = Mat(3, 1, CV_64FC1, v2);  //������
	V1 = Mat(3, 1, CV_64FC1, v1);  //������
	V1 = H * V2;
	corners.right_bottom.x = v1[0] / v1[2];
	corners.right_bottom.y = v1[1] / v1[2];
}

int main()
{
	Mat img = imread(IMAGE_PATH_PREFIX + "orb2.jpg");
	imgs.push_back(img);
	img = imread(IMAGE_PATH_PREFIX + "orb1.jpg");
	imgs.push_back(img);
	//img = imread(IMAGE_PATH_PREFIX + "boat3.jpg");
	//imgs.push_back(img);
	//img = imread(IMAGE_PATH_PREFIX + "boat3.jpg");
	//imgs.push_back(img);
	//img = imread(IMAGE_PATH_PREFIX + "boat4.jpg");
	//imgs.push_back(img);
	//img = imread(IMAGE_PATH_PREFIX + "boat5.jpg");
	//imgs.push_back(img);
	//img = imread(IMAGE_PATH_PREFIX + "boat6.jpg");
	//imgs.push_back(img);
	//�Ҷ�ͼƬת��
	vector<Mat> cvt_imgs;
	for (int i = 0; i < imgs.size(); i++)
	{
		Mat temp;
		cvtColor(imgs[i], temp, CV_RGB2GRAY);
		cvt_imgs.push_back(temp);
	}

	//��ȡ������
	OrbFeatureDetector orbFeatureDetector(1000);//�ο�opencvorbʵ�֣��������ȣ�ֵԽС����Խ��Խ��׼
	vector< vector<KeyPoint>> keyPoints;
	for (int i = 0; i < cvt_imgs.size(); i++)
	{
		vector<KeyPoint> keyPoint;
		orbFeatureDetector.detect(cvt_imgs[i], keyPoint);
		keyPoints.push_back(keyPoint);
	}

	//������������Ϊ������ƥ����׼��
	OrbDescriptorExtractor orbDescriptor;
	vector<Mat> imagesDesc;
	for (int i = 0; i < cvt_imgs.size(); i++)
	{
		Mat imageDesc;
		orbDescriptor.compute(cvt_imgs[i], keyPoints[i], imageDesc);
		imagesDesc.push_back(imageDesc);
	}
	flann::Index flannIndex(imagesDesc[0], flann::LshIndexParams(12, 20, 2), cvflann::FLANN_DIST_HAMMING);
	vector<DMatch> GoodMatchPoints;
	Mat matchIndex(imagesDesc[1].rows, 2, CV_32SC1), matchDistance(imagesDesc[1].rows, 2, CV_32FC1);
	flannIndex.knnSearch(imagesDesc[1], matchIndex, matchDistance, 2, flann::SearchParams());
	

	//����Lowe's �㷨ѡȡ����ƥ���
	for (int i = 0; i < matchDistance.rows; i++)
	{
		if (matchDistance.at<float>(i, 0) < 0.6*matchDistance.at<float>(i, 1))
		{
			DMatch dmatches(i, matchIndex.at<int>(i, 0), matchDistance.at<float>(i, 0));
			GoodMatchPoints.push_back(dmatches);
		}
	}
	//��goodmatch�㼯����ת��
	vector<vector<Point2f>> Points2f;
	vector<Point2f> imagePoint1, imagePoint2;
	Points2f.push_back(imagePoint1);
	Points2f.push_back(imagePoint2);
	vector<KeyPoint> keyPoint1, keyPoint2;
	keyPoint1=keyPoints[0];
	keyPoint2 = keyPoints[1];
		for (int i = 0; i < GoodMatchPoints.size(); i++)
		{
			Points2f[1].push_back(keyPoint2[GoodMatchPoints[i].queryIdx].pt);
			Points2f[0].push_back(keyPoint1[GoodMatchPoints[i].trainIdx].pt);
		}
	//��ȡͼ��1��ͼ��2 ��Ͷ����󣬳ߴ�Ϊ3*3
	Mat homo = findHomography(Points2f[0], Points2f[1], CV_RANSAC);//��Ҫlegacy.hppͷ�ļ�
	cout << "�任����Ϊ��\n" << homo << endl << endl;//���ӳ�����

	//������׼ͼ���ĸ����궥��
	CalcCorners(homo, imgs[0]);

	//ͼ��ƥ��
	vector<Mat> imagesTransform;
	Mat imageTransform1, imageTransform2;
	warpPerspective(imgs[0], imageTransform1, homo, Size(MAX(corners.right_top.x, corners.right_bottom.x), imgs[1].rows));
	imagesTransform.push_back(imageTransform1);
	imagesTransform.push_back(imageTransform2);
	imshow("ֱ�Ӿ���͸�Ӿ���任", imageTransform1);
	imwrite(IMAGE_PATH_PREFIX + "orb_transform_result.jpg", imageTransform1);

	//ͼ�񿽱�
	int dst_width = imageTransform1.cols;
	int dst_height = imgs[1].rows;
	Mat dst(dst_height, dst_width, CV_8UC3);
	dst.setTo(0);
	imageTransform1.copyTo(dst(Rect(0, 0, imageTransform1.cols, imageTransform1.rows)));
	imgs[1].copyTo(dst(Rect(0, 0, imgs[1].cols, imgs[1].rows)));
	imshow("copy_dst", dst);
	imwrite(IMAGE_PATH_PREFIX + "copy_dst.jpg", dst);
	Mat first_match;
	drawMatches(imgs[1], keyPoints[1], imgs[0], keyPoints[0], GoodMatchPoints, first_match);
	imshow("first_match", first_match);
	string match_result = IMAGE_PATH_PREFIX + "match_result.jpg";
	imwrite(match_result, first_match);
	waitKey();
	Mat pano;//ƴ�ӽ��ͼƬ
	ORB orb;
	Stitcher stitcher = Stitcher::createDefault(true);
	Stitcher::Status status = stitcher.stitch(imgs, pano);

	if (status != Stitcher::OK)
	{
		cout << "Can't stitch images, error code = " << int(status) << endl;
		return -1;
	}

	imwrite(result_name, pano);
	waitKey();
}
//�Ż���ͼ�����Ӵ���ʹ��ƴ����Ȼ
void OptimizeSeam(Mat& img1, Mat& trans, Mat& dst)
{
	int start = MIN(corners.left_top.x, corners.left_bottom.x);//��ʼλ�ã����ص��������߽�  

	double processWidth = img1.cols - start;//�ص�����Ŀ��  
	int rows = dst.rows;
	int cols = img1.cols; //ע�⣬������*ͨ����
	double alpha = 1;//img1�����ص�Ȩ��  
	for (int i = 0; i < rows; i++)
	{
		uchar* p = img1.ptr<uchar>(i);  //��ȡ��i�е��׵�ַ
		uchar* t = trans.ptr<uchar>(i);
		uchar* d = dst.ptr<uchar>(i);
		for (int j = start; j < cols; j++)
		{
			//�������ͼ��trans�������صĺڵ㣬����ȫ����img1�е�����
			if (t[j * 3] == 0 && t[j * 3 + 1] == 0 && t[j * 3 + 2] == 0)
			{
				alpha = 1;
			}
			else
			{
				//img1�����ص�Ȩ�أ��뵱ǰ�������ص�������߽�ľ�������ȣ�ʵ��֤�������ַ���ȷʵ��  
				alpha = (processWidth - (j - start)) / processWidth;
			}

			d[j * 3] = p[j * 3] * alpha + t[j * 3] * (1 - alpha);
			d[j * 3 + 1] = p[j * 3 + 1] * alpha + t[j * 3 + 1] * (1 - alpha);
			d[j * 3 + 2] = p[j * 3 + 2] * alpha + t[j * 3 + 2] * (1 - alpha);

		}
	}

}

