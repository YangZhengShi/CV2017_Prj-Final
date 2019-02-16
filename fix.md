���ɺ�Ĵ��룺

```
/**
* ͼ��ȫ��ƴ��
@author л֮ƽ
*/
#include <iostream>  
#include <string>  
#include "opencv2/highgui/highgui.hpp"  
#include <opencv2/imgproc/imgproc.hpp> 
#include "opencv2/core/core.hpp"  
#include<stdlib.h>
#include <opencv2/stitching/stitcher.hpp>
#include"opencv2/nonfree/nonfree.hpp"
#include"opencv2/legacy/legacy.hpp"
#include <io.h>  

using namespace std;
using namespace cv;

typedef struct
{
	Point2f left_top;
	Point2f left_bottom;
	Point2f right_top;
	Point2f right_bottom;
}four_corners_t;

typedef struct
{
	int src_pic_num; // ͼƬ���
	int dst_pic_num;
	float src_pic_x; // ƽ��x����
	float dst_pic_x;
	vector<DMatch> good_match_points;
}good_macth_pair;


void OptimizeSeam(Mat &img1, Mat &trans, Mat& dst);
four_corners_t corners;
string IMAGE_PATH_PREFIX = "./image/";
bool try_use_gpu = false;
vector<Mat> imgs;
string result_name = IMAGE_PATH_PREFIX + "result.jpg";
vector<good_macth_pair> good_macth_pairs;
int book[100] = { 0 }; // ���ڱ��ͼƬ�Ƿ��޳�������
/**
* ����ͼƬ�ĸ��ǵ�ֵ�ķ���
*/
void CalcCorners(const Mat& H, const Mat& src)
{
	double v2[] = { 0, 0, 1 };//���Ͻ�
	double v1[3];//�任�������ֵ
	Mat V2 = Mat(3, 1, CV_64FC1, v2);  //������
	Mat V1 = Mat(3, 1, CV_64FC1, v1);  //������

	V1 = H * V2;
	//���Ͻ�(0,0,1)
	cout << "\nHomography: " << H << endl;
	cout << "\n������V2: " << V2 << endl;
	cout << "\n������V1: " << V1 << endl;
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

/**
* ��ȡָ���ļ���������ͼƬ�ķ���
*/
int readImg()
{
	cout << "\n*******��ʼ����ͼƬ*******" << endl;
	int i = 1;
	const char path[100] = "./test/*.jpg";
	struct _finddata_t fileinfo;
	intptr_t handle; // ���ﲻ����long
	handle = _findfirst(path, &fileinfo);
	if (!handle)
	{
		cout << "�����·���д���" << endl;
		return -1;
	}
	else {
		string name = fileinfo.name;
		cout << "\n��" << i << "��ͼƬ��" << fileinfo.name << endl;
		Mat img = imread("./test/" + name);
		// �ߴ����  
		resize(img, img, Size(400, 300), 0, 0, INTER_LINEAR);
		imgs.push_back(img);
		i++;
		while (!_findnext(handle, &fileinfo))
		{
			cout << "\n��" << i << "��ͼƬ��" << fileinfo.name << endl;
			string name = fileinfo.name;
			Mat img = imread("./test/" + name);
			// �ߴ����  
			resize(img, img, Size(400, 300), 0, 0, INTER_LINEAR);
			imgs.push_back(img);
			i++;
		}
	}
	if (_findclose(handle) == 0) cout << "\n�ļ�����ɹ��ر�" << endl;  // ��Ҫ���˹رվ��  
	else cout << "\n�ļ�����ر�ʧ��..." << endl;
	return 0;
}

/**
* ���������������������ͼƬ���飬�洢����ʾ���޳���ͼƬ
*/
vector<set<int>> group(vector<Mat>& imagesDesc, vector<vector<KeyPoint>>& keyPoints)
{
	vector<set<int>> groups; // �洢ͼƬ�������
	for (int i = 0; i < imgs.size(); i++)
	{
		for (int j = i + 1; j < imgs.size(); j++)
		{
			flann::Index flannIndex(imagesDesc[i], flann::LshIndexParams(12, 20, 2), cvflann::FLANN_DIST_HAMMING);
			vector<DMatch> GoodMatchPoints;
			Mat matchIndex(imagesDesc[j].rows, 2, CV_32SC1), matchDistance(imagesDesc[j].rows, 2, CV_32FC1);
			flannIndex.knnSearch(imagesDesc[j], matchIndex, matchDistance, 2, flann::SearchParams());
			// ����ͼƬ˳�������Ӱ��������Եļ���
			// ����Lowe's �㷨ѡȡ����ƥ���
			for (int k = 0; k < matchDistance.rows; k++)
			{
				if (matchDistance.at<float>(k, 0) < 0.6*matchDistance.at<float>(k, 1))
				{
					DMatch dmatches(k, matchIndex.at<int>(k, 0), matchDistance.at<float>(k, 0));
					GoodMatchPoints.push_back(dmatches);
				}
			}
			good_macth_pair gp; // ��¼����ƥ���ԣ������ظ�����
			gp.src_pic_num = i;
			gp.dst_pic_num = j;
			gp.src_pic_x = 0; // ��ʼ��
			gp.dst_pic_x = 0;
			float src_pic_x_sum = 0;
			float dst_pic_x_sum = 0;
			gp.good_match_points = GoodMatchPoints;
			for (int k = 0; k < GoodMatchPoints.size(); k++)
			{
				DMatch dmatch = GoodMatchPoints[k];
				src_pic_x_sum += keyPoints[i][dmatch.trainIdx].pt.x;
				dst_pic_x_sum += keyPoints[j][dmatch.queryIdx].pt.x;
			}
			if (GoodMatchPoints.size() != 0){
				gp.src_pic_x = src_pic_x_sum / GoodMatchPoints.size();
				gp.dst_pic_x = dst_pic_x_sum / GoodMatchPoints.size();
			}
			good_macth_pairs.push_back(gp);
			
			
			// ����ƥ�����������10��ͼƬ��Ϊһ��
			if (GoodMatchPoints.size() > 10)
			{
				int insertFlag = 0;
				for (int k = 0; k < groups.size(); k++)
				{
					if (groups[k].count(i) > 0) // �����Ϊi����
					{
						groups[k].insert(j);
						insertFlag = 1;
						break;
					}
					else if (groups[k].count(j) > 0) // �����Ϊj����
					{
						groups[k].insert(i);
						insertFlag = 1;
						break;
					}
				}
				if (insertFlag == 0) // ��δΪi��j���飬��������
				{
					set<int> group;
					group.insert(i);
					group.insert(j);
					groups.push_back(group);
				}
			}
		}
	}

	for (set<int> group : groups)
	{
		cout << "\n����:\n";
		for (int pic : group)
		{
			cout << pic << " ";
			if (pic >= 0 && book[pic] == 0) {
				book[pic] = 1; // ���Ѿ��ڷ����е�ͼƬ���Ϊ1�����Ϊ0��С��С��imgs������ͼƬ��Ϊ���޳���ͼƬ
			}
		}
		cout << endl;
	}
	return groups;
}

int vectorSearch(vector<int>& v, int num)
{
	vector <int>::iterator iElement = find(v.begin(),
		v.end(), num);
	if (iElement != v.end())
	{
		int nPosition = distance(v.begin(), iElement);
		return nPosition;
	}
	return -1;
}


/**
* ����ͼƬӳ���������Ϣ����ͼƬ������������
*/
vector<vector<int>> order(vector<set<int>>& groups, vector<good_macth_pair>& good_macth_pairs)
{
	vector<vector<int>> ordered_groups;
	for (int i = 0; i < groups.size();i++) 
	{
		cout << "\nͼƬ˳��������:\n";
		int size = groups[i].size();

		set<int> group = groups[i];
		map<int, int> ordered_pic;
		set<int>::iterator iter;
		for (iter = group.begin(); iter != group.end(); iter++)
		{
			int pic1_num = *iter;
			int pic2_num = -1;
			int min_big_x = imgs[pic1_num].cols;
			for (good_macth_pair gp : good_macth_pairs)
			{
				if (gp.src_pic_num == pic1_num && group.count(gp.dst_pic_num) > 0) // ��j��ͼ������е���һ��ͼ�Ƚ�
				{
					if (gp.dst_pic_x > gp.src_pic_x)
					{
						if (gp.dst_pic_x < min_big_x)
						{
							pic2_num = gp.dst_pic_num;
							min_big_x = gp.dst_pic_x;
			
						}
					}
				}
				else if (gp.dst_pic_num == pic1_num &&  group.count(gp.src_pic_num) > 0)
				{
					if (gp.dst_pic_x < gp.src_pic_x)
					{
						if (gp.src_pic_x < min_big_x)
						{
							pic2_num = gp.src_pic_num;
							min_big_x = gp.src_pic_x;
						}
					}
				}
			}
			ordered_pic[pic1_num] = pic2_num;
			cout << pic1_num << " " << pic2_num << endl;
		}

		vector<int> o_group;
		cout << "\n���������:\n";

		for (iter = group.begin(); iter != group.end(); iter++)
		{
			int pic = *iter;
			if (vectorSearch(o_group, pic) == -1) // ֻҪ�Ѳ������ͼ�����ǰ�档Ȼ���һ������ͼ����Ŷ��ӽ���
			{
				int index = 0;
				o_group.insert(o_group.begin(), pic);
				
				
				while (ordered_pic[pic] != -1 && vectorSearch(o_group, ordered_pic[pic]) == -1)
				{
					index += 1;
					o_group.insert(o_group.begin()+index, ordered_pic[pic]);
					pic = ordered_pic[pic];
					
				}
			}

		}
		for (int t : o_group)
		{

			cout << t << " ";
		}
		cout << endl;
		ordered_groups.push_back(o_group);
	}
	return ordered_groups;
}
/*
* �������ܣ��������ž����ص���ͼƬ���ϳɲ��Ż�����һ��ͼƬ����
*/
Mat Stitched(Mat img1, Mat img2)
{
	vector<Mat> s_imgs;
	//����debug
	Mat img = img1;
	s_imgs.push_back(img);
	img = img2;
	s_imgs.push_back(img);
	
	//�Ҷ�ͼƬת��
	vector<Mat> cvt_s_imgs;
	for (int i = 0; i < s_imgs.size(); i++)
	{
		Mat temp;
		cvtColor(s_imgs[i], temp, CV_RGB2GRAY);
		cvt_s_imgs.push_back(temp);
	}

	//��ȡ������
	OrbFeatureDetector orbFeatureDetector(1000);//�ο�opencvorbʵ�֣��������ȣ�ֵԽС����Խ��Խ��׼
	vector< vector<KeyPoint>> keyPoints;
	for (int i = 0; i < cvt_s_imgs.size(); i++)
	{
		vector<KeyPoint> keyPoint;
		orbFeatureDetector.detect(cvt_s_imgs[i], keyPoint);
		keyPoints.push_back(keyPoint);
	}

	//������������Ϊ������ƥ����׼��
	OrbDescriptorExtractor orbDescriptor;
	vector<Mat> imagesDesc;
	for (int i = 0; i < cvt_s_imgs.size(); i++)
	{
		Mat imageDesc;
		orbDescriptor.compute(cvt_s_imgs[i], keyPoints[i], imageDesc);
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
	keyPoint1 = keyPoints[0];
	keyPoint2 = keyPoints[1];
	for (int i = 0; i < GoodMatchPoints.size(); i++)
	{
		Points2f[1].push_back(keyPoint2[GoodMatchPoints[i].queryIdx].pt);
		Points2f[0].push_back(keyPoint1[GoodMatchPoints[i].trainIdx].pt);
	}
	Mat match;
	drawMatches(img1, keyPoint2, img2, keyPoint1, GoodMatchPoints, match);
	imshow("match", match);
	waitKey();
	//��ȡͼ��1��ͼ��2 ��Ͷ����󣬳ߴ�Ϊ3*3
	Mat homo = findHomography(Points2f[0], Points2f[1], CV_RANSAC);//��Ҫlegacy.hppͷ�ļ�

	//������׼ͼ���ĸ����궥��
	CalcCorners(homo, s_imgs[0]);

	//ͼ��ƥ��
	vector<Mat> imagesTransform;
	Mat imageTransform1, imageTransform2;
	warpPerspective(s_imgs[0], imageTransform1, homo, Size(MAX(corners.right_top.x, corners.right_bottom.x), s_imgs[1].rows));
	imagesTransform.push_back(imageTransform1);
	imagesTransform.push_back(imageTransform2);
	//imshow("ֱ�Ӿ���͸�Ӿ���任", imageTransform1);

	//ͼ�񿽱�
	int dst_width = imageTransform1.cols;
	int dst_height = s_imgs[1].rows;
	Mat dst(dst_height, dst_width, CV_8UC3);
	dst.setTo(0);
	imageTransform1.copyTo(dst(Rect(0, 0, imageTransform1.cols, imageTransform1.rows)));
	s_imgs[1].copyTo(dst(Rect(0, 0, s_imgs[1].cols, s_imgs[1].rows)));

	//�Ż�ƴ�ӱ�Ե
	OptimizeSeam(s_imgs[1], imageTransform1, dst);
	//imshow("��Ե�Ż����", dst);
	// waitKey();
	return dst;
}

int main()
{
	cout << CV_VERSION << endl;
	/**
	* ��һ���� �Ӵ��̶���ͼƬ
	*/
	readImg();
	/**
	* �ڶ����� Ԥ����ͼƬ���лҶ�ת��
	*/
	vector<Mat> cvt_imgs;
	for (int i = 0; i < imgs.size(); i++)
	{
		Mat temp;
		cvtColor(imgs[i], temp, CV_RGB2GRAY);
		cvt_imgs.push_back(temp);
	}
	
	/**
	* �������� ��ȡ������
	*/
	OrbFeatureDetector orbFeatureDetector(1000); // �ο�opencvorbʵ�֣��������ȣ�ֵԽС����Խ��Խ��׼
	vector<vector<KeyPoint>> keyPoints;
	for (int i = 0; i < cvt_imgs.size(); i++)
	{
		vector<KeyPoint> keyPoint;
		orbFeatureDetector.detect(cvt_imgs[i], keyPoint);
		keyPoints.push_back(keyPoint);
	}

	/**
	* ���Ĳ���������������Ϊ������ƥ����׼��
	*/
	OrbDescriptorExtractor orbDescriptor;
	vector<Mat> imagesDesc;
	for (int i = 0; i < cvt_imgs.size(); i++)
	{
		Mat imageDesc;
		orbDescriptor.compute(cvt_imgs[i], keyPoints[i], imageDesc);
		imagesDesc.push_back(imageDesc);
	}

	/**
	* ���岽�� ���������㽫һ������ͼƬ���з���
	*/
	vector<set<int>> groups = group(imagesDesc, keyPoints);
	
	/**
	* �����������򣬰��պõ�ƥ�����x�����ƽ��ֵ�Ĵ�С������ͼƬ������˳��
	*/
	vector<vector<int>> ordered_groups = order(groups, good_macth_pairs); 
	/**
	* ���߲��� ���������źõ�˳������ƴ��ͼƬ
	*/
	for (int i = 0; i < ordered_groups.size(); i++)
	{
		cout << "\n*******����ƴ�ӵ�" << i + 1 << "��ȫ��ͼ*******" << endl;
		vector<int> temp_ordered = ordered_groups[i];
		// ��һ��ͼƬ����ƴ��
		int temp_index = temp_ordered[0];
		Mat img1 = imgs[temp_index];
		for (int j = 1; j < temp_ordered.size(); j++)
		{
			int index = temp_ordered[j];
			img1 = Stitched(img1, imgs[index]);

		}
		cout << "\n��" << i << "��ȫ��ͼƴ�����" << endl;
		imshow("ȫ��ͼ", img1);
		waitKey();
		stringstream stream;
		string str;
		stream << i;
		stream >> str;
		string filename = "./result/ȫ��ͼ"+ str + ".jpg";
		if (i > 0) {
			imwrite(filename, img1);
		}
	}
	/**
	* �ڰ˲����ҵ��������ͼƬ����ʾ���洢
	*/
	for (int i = 0; i < imgs.size(); i++) {
		if (book[i] == 0) {
			stringstream ss;
			string name;
			ss << i;
			ss >> name;
			string filename = "./result/exImg" + name + ".jpg";
			imwrite(filename, imgs[i]);
			imshow("���޳���ͼƬ", imgs[i]);
			waitKey();
		}
	}
}

//�������Ż��ӷ��Ӿ���
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

```