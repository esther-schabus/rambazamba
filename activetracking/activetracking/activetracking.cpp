#include "stdafx.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <curl\curl.h>
#include <Windows.h>

using namespace std;
using namespace cv;

const int LEFT = 1;
const int RIGHT = 2;
const int UP = 3;
const int DOWN = 4;

CascadeClassifier front_face_cascade;
CascadeClassifier side_face_cascade;
string window_name = "Face Detection";
int camera_zoom = 0; // zoom position
int camera_lr = 0;	// left/right position
int camera_ud = 0;	// up/down position

Point getImgCenter(VideoCapture cap);
Rect detectFace(Mat frame);
void drawBoundingBox(Rect faces, Mat frame);
Rect chooseFace(vector<Rect> faces);
Point getFacePos(Rect face);
bool moveFaceToCenter(VideoCapture cap, Rect face, CURL* curl);
bool zoomFace(VideoCapture cap, Rect face, CURL* curl);
void zoomIn(CURL* curl);
void zoomOut(CURL* curl);
bool lookLeft(CURL* curl);
bool lookRight(CURL* curl);
void lookUp(CURL* curl);
void lookDown(CURL* curl);
bool checkTolerance(Rect face, Rect tolerance);
void patrol(CURL* curl, VideoCapture cap);
void initCurl(CURL* curl);
void followFace(CURL* curl, VideoCapture cap);
bool contains(vector<int> vec, int element);
vector<int> checkMovement(Rect old_face, Rect new_face);


int main()
{
	// init camera control
	CURL *curl = curl_easy_init();
	if (!curl)
		return -1;
	initCurl(curl);

	//init VideoCapture
	VideoCapture cap("http://admin:password@192.168.0.115:80/video.mjpg");
	if (!cap.isOpened())
		return -1;
	if (!front_face_cascade.load("haarcascade_frontalface_alt2.xml"))
		return -1;
	if (!side_face_cascade.load("haarcascade_profileface.xml"))
		return -1;
	
	
	namedWindow("vid", WINDOW_NORMAL);
	patrol(curl, cap);
	followFace(curl, cap);

	destroyAllWindows();
    return 0;
}

Point getImgCenter(VideoCapture cap) {
	Point center;
	center.x = cap.get(CAP_PROP_FRAME_WIDTH) * 0.5;
	center.y = cap.get(CAP_PROP_FRAME_HEIGHT) * 0.5;
	return center;
}

Rect detectFace(Mat frame) {
	vector<Rect> faces;
	Mat frame_gray;
	cvtColor(frame, frame_gray, COLOR_BGR2GRAY); //convert to grayscale
	equalizeHist(frame_gray, frame_gray); //equalize histogram
	front_face_cascade.detectMultiScale(frame_gray, faces, 1.2, 6, 0 | CV_HAAR_SCALE_IMAGE, Size(20, 20));	// frontal face
	if (faces.empty()) {
		side_face_cascade.detectMultiScale(frame_gray, faces, 1.3, 7, 0 | CV_HAAR_SCALE_IMAGE, Size(20, 20)); // face looks to the right side (from face point of view)
	}
	if (faces.empty()) {
		flip(frame_gray, frame_gray, 1);
		side_face_cascade.detectMultiScale(frame_gray, faces, 1.3, 7, 0 | CV_HAAR_SCALE_IMAGE, Size(20, 20)); // face looks to the left side (from face point of view)
	}
	Rect face = chooseFace(faces);
	drawBoundingBox(face, frame);
	return face;
}

void drawBoundingBox(Rect face, Mat frame) {
	int x = face.x;
	int y = face.y;
	int w = x + face.width;
	int h = y + face.height;
	rectangle(frame, Point(x, y), Point(w, h), Scalar(255, 255, 255), 2, 8, 0);
	imshow(window_name, frame);
}

Rect chooseFace(vector<Rect> faces) {
	Rect chosen_face(0,0,0,0);
	for (size_t i = 0; i < faces.size(); i++) {
		Rect current_face = faces[i];
		if (current_face.width > chosen_face.width)
			chosen_face = current_face;
	}
	return chosen_face;
}

Point getFacePos(Rect face) {
	return (face.br() + face.tl())*0.5;
}

bool moveFaceToCenter(VideoCapture cap, Rect face, CURL* curl) {
	bool moved = false;
	Point face_center = getFacePos(face);
	Point img_center = getImgCenter(cap);
	auto img_width = cap.get(CAP_PROP_FRAME_WIDTH);
	auto img_height = cap.get(CAP_PROP_FRAME_HEIGHT);
	if (face_center.x + img_width * 0.1 < img_center.x) {
		lookLeft(curl);
		cout << face_center.x << " < " << img_center.x << " --> lookLeft" << endl;
		moved = true;
	}
	else if (face_center.x > img_center.x + img_width * 0.1) {
		lookRight(curl);
		cout << face_center.x << " > " << img_center.x << " --> lookRight" << endl;
		moved = true;
	}
	if (face_center.y > img_center.y + img_height * 0.1) {
		lookDown(curl);
		cout << face_center.y << " > " << img_center.y << " --> lookDown" << endl;
		moved = true;
	}
	else if (face_center.y + img_height * 0.1 < img_center.y) {
		lookUp(curl);
		cout << face_center.y << " < " << img_center.y << " --> lookUp" << endl;
		moved = true;
	}
	return moved;
}

bool zoomFace(VideoCapture cap, Rect face, CURL* curl) {
	bool zoomed = false;
	if (cap.get(CAP_PROP_FRAME_HEIGHT) / face.height > 7 && camera_zoom < 3) {
		zoomIn(curl);
		zoomed = true;
	}
	else if (cap.get(CAP_PROP_FRAME_HEIGHT) / face.height < 3 && camera_zoom > 0) {
		zoomOut(curl);
		zoomed = true;
	}
	return zoomed;
}

void zoomIn(CURL* curl) {
	if (camera_zoom < 3) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "zoom=tele");
		curl_easy_perform(curl);
		camera_zoom++;
	}
}

void zoomOut(CURL* curl) {
	if (camera_zoom > 0) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "zoom=wide");
		curl_easy_perform(curl);
		camera_zoom--;
	}

}

bool lookLeft(CURL* curl) {
	if (camera_lr > -21) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "move=left");
		curl_easy_perform(curl);
		camera_lr--;
		return true;
	}
	return false;
}

bool lookRight(CURL* curl) {
	if (camera_lr < 21) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "move=right");
		curl_easy_perform(curl);
		camera_lr++;
		return true;
	}
	return false;
}

void lookUp(CURL* curl) {
	if (camera_ud < 50) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "move=up");
		curl_easy_perform(curl);
		camera_ud++;
	}
}
void lookDown(CURL* curl) {
	if (camera_ud > -8) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "move=down");
		curl_easy_perform(curl);
		camera_ud--;
	}

}

bool checkTolerance(Rect face, Rect tolerance) {
	if (face.x < tolerance.x)
		return false;
	if (face.y < tolerance.y)
		return false;
	if ((face.x + face.width) > (tolerance.x + tolerance.width))
		return false;
	if ((face.y + face.height) > (tolerance.y + tolerance.height))
		return false;
	return true;
}

void patrol(CURL* curl, VideoCapture cap) {
	bool left = true;
	bool right = true;
	Mat frame;
	Rect face;
	int count = 3;

	while (left || right) {
		cap >> frame;
		if (frame.empty())
			break;
		imshow(window_name, frame);
		if (count == 3) {
			face = detectFace(frame);
			count = 0;
		}	
		if (!face.empty())
			break;
		if (waitKey(10) == 27)			//stop if "Esc" is pressed
			break;
		if (left) {
			left = lookLeft(curl);
		}
		if (!left) {
			right = lookRight(curl);
			if (!right)
				left = true;
		}
		Sleep(200);
		count++;
	}
}

void initCurl(CURL* curl) {
	curl_easy_setopt(curl, CURLOPT_URL, "http://admin:password@192.168.0.115:80/cgi-bin/viewer/camctrl.cgi");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "move=home");
	curl_easy_perform(curl);
	Sleep(5000);
}

void followFace(CURL* curl, VideoCapture cap) {
	Mat frame;
	Rect old_face;
	Rect face;
	vector<int> movements;
	Rect tolerance(getImgCenter(cap).x - cap.get(CAP_PROP_FRAME_WIDTH) / 4, getImgCenter(cap).y - cap.get(CAP_PROP_FRAME_HEIGHT) / 4, cap.get(CAP_PROP_FRAME_WIDTH) / 2, cap.get(CAP_PROP_FRAME_HEIGHT) / 2);
	bool detect = true;
	bool moved = false;
	bool zoomed = false;
	int count = 10;
	while (true) {
		cap >> frame;
		if (frame.empty())
			break;
		drawBoundingBox(tolerance, frame);
		if (detect && (count == 10)) {
			old_face = face;
			face = detectFace(frame);
			if (face.empty())
				cout << "No face detected :(" << endl;
			if (!face.empty()) {
				detect = false;
				if (!old_face.empty() && !moved && !zoomed) {
					movements = checkMovement(old_face, face);
					if (movements.empty())
						cout << "no movement!" << endl;
					else {
						if (contains(movements, RIGHT))
							cout << "face moved right" << endl;
						if (contains(movements, LEFT))
							cout << "face moved left" << endl;
						if (contains(movements, UP))
							cout << "face moved up" << endl;
						if (contains(movements, DOWN))
							cout << "face moved down" << endl;
					}
				}
			}
			count = 0;
		}
		if (waitKey(10) == 27)			//stop if "Esc" is pressed
			break;
		if (!detect) {
			if (!checkTolerance(face, tolerance)) {
				if (moveFaceToCenter(cap, face, curl)) {
					cout << "moved" << endl;
					moved = true;
				}
			}
			else moved = false;
			if (zoomFace(cap, face, curl)) {
				cout << "zoomed" << endl;
				zoomed = true;
			}
			else zoomed = false;
			detect = true;
		}
		count++;
	}
}

vector<int> checkMovement(Rect old_face, Rect new_face) {
	vector<int> movements;
	Point last_pos = getFacePos(old_face);
	Point current_pos = getFacePos(new_face);
	if (last_pos.x < current_pos.x && abs(last_pos.x - current_pos.x) > 6)
		movements.push_back(RIGHT);
	if (last_pos.x > current_pos.x && abs(last_pos.x - current_pos.x) > 6)
		movements.push_back(LEFT);
	if (last_pos.y > current_pos.y && abs(last_pos.y - current_pos.y) > 6)
		movements.push_back(UP);
	if (last_pos.y < current_pos.y && abs(last_pos.y - current_pos.y) > 6)
		movements.push_back(DOWN);
	return movements;
}

bool contains(vector<int> vec, int element) {
	if (find(vec.begin(), vec.end(), element) != vec.end())
		return true;
	else return false;
}