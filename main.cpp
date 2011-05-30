
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cmath>
#include <cassert>
#include <vector>

#include <fltk/FL_API.h>
#include <fltk/InvisibleBox.h>
#include <fltk/file_chooser.h>
#include <fltk/filename.h>
#include <fltk/SharedImage.h>
#include <fltk/Image.h>
#include <fltk/ask.h>
#include <fltk/ValueInput.h>
#include <fltk/ReturnButton.h>
#include <fltk/ProgressBar.h>
#include <fltk/MenuBuild.h>
#include <fltk/Window.h>
#include <fltk/run.h>

using namespace std;
using namespace fltk;


extern Image* inpaint_fast_marching(const Image*, const Image*);
extern Image* inpaint_criminisi(const Image*, const Image*);

static ProgressBar* bar = NULL;
class DisplayWidget;
static DisplayWidget* image_box = NULL;
static Image* img = NULL;
static Image* oldimg = NULL;
static Image* mask = NULL;
static vector<Image*> images;
static const int brush_size = 10;

class DisplayWidget : public InvisibleBox
{
public:
	DisplayWidget(int x, int y, int w, int h) : InvisibleBox(x, y, w, h) { };
	int handle(int ev)
	{
		if(ev == ENTER)
			return 1;
		if(ev == DRAG)
		{
			if(!img)
				return 0;
			draw_pix(event_x() - (w() - img->buffer_width())/2, event_y() - (h() - img->buffer_height())/2);
			return 1;
		}
		if(ev == PUSH)
		{
			if(!img)
				return 0;
			if(!oldimg)
			{
				oldimg = new Image;
				img->forceARGB32();
				oldimg->setimage( img->buffer(),
					RGB32,
					img->buffer_width(),
					img->buffer_height() );

				if(!mask)
					mask = new Image;
				mask->setsize(img->buffer_width(), img->buffer_height());
				uchar pixel[4] = {255, 255, 255, 0};
				for(int y = 0; y < img->buffer_height(); y++)
					for(int x = 0; x < img->buffer_width(); x++)						
						mask->setpixels(&pixel[0], Rectangle(x, y, 1, 1));
			}
			draw_pix(event_x() - (w() - img->buffer_width())/2, event_y() - (h() - img->buffer_height())/2);
			return 1;
		}
		if(ev == RELEASE)
		{
			if(!img)
				return 0;
			return 1;
		}
		return InvisibleBox::handle(ev);
	}
	
	void draw_pix(int x, int y)
	{
		if(x < 0 || y < 0 || x > img->buffer_width() || y > img->buffer_height())
			return;
		for(int i = x - brush_size / 2; i < x + brush_size / 2; i++)
			for(int j = y - brush_size / 2; j < y + brush_size / 2; j++)
			{
				if(i < 0)
					i = 0;
				if(i >= img->buffer_width())
					break;
				if(j < 0)
					j = 0;
				if(j >= img->buffer_height())
					break;
				uchar newpixel[4 * brush_size * brush_size];
				memset(newpixel, 0, sizeof(newpixel));
				img->setpixels(&newpixel[0], Rectangle(i, j, 1, 1));
				mask->setpixels(&newpixel[0], Rectangle(i, j, 1, 1));
				image_box->redraw();
			}
	}
};

void open_cb(Widget*, void*)
{
	const char* filename = file_chooser("Select image file to open",
		"Image Files (*.{bmp,jpg,png})",
		".");

	if(!filename)
		return;
	if(img)
		images.push_back(img);
	img = NULL;
	const char* extension = filename_ext(filename);
	if(strcmp(extension, ".bmp") == 0)
		img = bmpImage::get(filename);
	if(strcmp(extension, ".jpg") == 0)
		img = jpegImage::get(filename);
	if(strcmp(extension, ".png") == 0)
		img = pngImage::get(filename);

	if(!img)
	{
		message("%s: wrong format", filename);
		return;
	}
	image_box->image(img);
	image_box->redraw();
}

void quit_cb(Widget*, void*)
{
	exit(0);
}

void undo_cb(Widget*, void*)
{
	if(images.size() != 0)
	{
		delete img;
		img = images.back();
		images.pop_back();
		image_box->image(img);
		image_box->redraw();
	}
	else
	{
		alert("Nothing to undo");
	}
}

static bool working = false;

void sliding_avg_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	const char* Nstr = input("Input window size", "10");
	if(!Nstr)
		return;
	for(int i = 0; i < strlen(Nstr); i++)
		if(!isdigit(Nstr[i]))
			return;
	int N = atoi(Nstr);
	if(N == 0)
		return;

	working = true;

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setimage( img->buffer(),
		RGB32,
		img->buffer_width(),
		img->buffer_height() );

	for(int y = 0; y < img->buffer_height(); y++)
	{
		bar->position(100.0 * (double) y / img->buffer_height());
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width(); x++)
		{
			unsigned long int rsum = 0, gsum = 0, bsum = 0;
			for(int j = -N/2; j <= N/2; j++)
				for(int i = -N/2; i <= N/2; i++)
				{
					int y_idx = y + j >= img->buffer_height() ?
						N - j:
						y + j;
					if(y_idx < 0)
						y_idx += img->buffer_height();
					int x_idx = x + i >= img->buffer_width() ?
						N - i:
						x + i;
					if(x_idx < 0)
						x_idx += img->buffer_width();

					size_t index = y_idx * img->buffer_width() * 4
						+ x_idx * 4;
					uchar r = img->buffer()[index + 0];
					rsum += r;
					uchar g = img->buffer()[index + 1];
					gsum += g;
					uchar b = img->buffer()[index + 2];
					bsum += b;
				}
			uchar newpixel[4];
			newpixel[0] = (rsum / (N * N)) > 255 ? 255 : uchar(rsum / (N * N));
			newpixel[1] = (gsum / (N * N)) > 255 ? 255 : uchar(gsum / (N * N));
			newpixel[2] = (bsum / (N * N)) > 255 ? 255 : uchar(bsum / (N * N));
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void upscale_nn_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	const char* Nstr = input("Scale factor", "2");
	if(!Nstr)
		return;
	for(int i = 0; i < strlen(Nstr); i++)
		if(!isdigit(Nstr[i]))
			return;
	int N = atoi(Nstr);
	if(N == 0)
		return;

	working = true;

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setsize(img->buffer_width() * N, img->buffer_height() * N);
	newimage->setpixeltype(RGB32);

	for(int y = 0; y < img->buffer_height() * N; y++)
	{
		bar->position(100.0 * (double) y / (img->buffer_height() * N));
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width() * N; x++)
		{
			size_t index = (y / N) * img->buffer_width() * 4
				+ (x / N) * 4;
			char r = img->buffer()[index + 0];
			char g = img->buffer()[index + 1];
			char b = img->buffer()[index + 2];

			uchar newpixel[4];
			newpixel[0] = r;
			newpixel[1] = g;
			newpixel[2] = b;
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void upscale_bl_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	const char* Nstr = input("Scale factor", "2");
	if(!Nstr)
		return;
	for(int i = 0; i < strlen(Nstr); i++)
		if(!isdigit(Nstr[i]))
			return;
	int N = atoi(Nstr);
	if(N == 0)
		return;

	working = true;

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setsize(img->buffer_width() * N, img->buffer_height() * N);
	newimage->setpixeltype(RGB32);

	for(int y = 0; y < img->buffer_height() * N; y++)
	{
		bar->position(100.0 * (double) y / (img->buffer_height() * N));
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width() * N; x++)
		{
			int floor_x = x / N,
				floor_y = y / N;
			int ceil_x = floor_x + 1;
			if(ceil_x >= img->buffer_width())
				ceil_x = floor_x;
			int ceil_y = floor_y + 1;
			if(ceil_y >= img->buffer_height())
				ceil_y = floor_y;
			double fraction_x = double(x) / N - floor_x,
				fraction_y = double(y) / N - floor_y;
			double one_minus_x = 1.0 - fraction_x,
				one_minus_y = 1.0 - fraction_y;

			uchar* f1 = &img->buffer()[floor_y * img->buffer_width() * 4 +
				floor_x * 4];
			uchar* f2 = &img->buffer()[floor_y * img->buffer_width() * 4 +
				ceil_x * 4];
			uchar* f3 = &img->buffer()[ceil_y  * img->buffer_width() * 4 +
				floor_x * 4];
			uchar* f4 = &img->buffer()[ceil_y  * img->buffer_width() * 4 +
				ceil_x * 4];

			uchar newpixel[4], p1, p2;
			p1 = uchar(one_minus_x * f1[0] + fraction_x * f2[0]);
			p2 = uchar(one_minus_x * f3[0] + fraction_x * f4[0]);
			newpixel[0] = uchar(one_minus_y * p1 + fraction_y * p2);
			p1 = uchar(one_minus_x * f1[1] + fraction_x * f2[1]);
			p2 = uchar(one_minus_x * f3[1] + fraction_x * f4[1]);
			newpixel[1] = uchar(one_minus_y * p1 + fraction_y * p2);
			p1 = uchar(one_minus_x * f1[2] + fraction_x * f2[2]);
			p2 = uchar(one_minus_x * f3[2] + fraction_x * f4[2]);
			newpixel[2] = uchar(one_minus_y * p1 + fraction_y * p2);

			// Список литературы
			// [1] http://www.codeproject.com/KB/GDI-plus/imageprocessing4.aspx

			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

Image* filter(double* kernel, int kern_height, int kern_width, const Image* img)
{
	assert(kern_height % 2);
	assert(kern_width % 2);  // должен быть средний элемент

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setsize(img->buffer_width(), img->buffer_height());
	newimage->setpixeltype(RGB32);

	for(int y = 0; y < img->buffer_height(); y++)
	{
		bar->position(100.0 * (double) y / (img->buffer_height()));
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width(); x++)
		{
			double newpix[3];
			memset(newpix, 0, sizeof(newpix));
			for(int j = -int(kern_height / 2); j <= kern_height / 2; j++)
			{
				int y_idx = j + y;
				if(y_idx < 0)
					y_idx += img->buffer_height();
				if(y_idx >= img->buffer_height())
					y_idx -= img->buffer_height();
				for(int i = -int(kern_width / 2); i <= kern_width / 2; i++)
				{
					int x_idx = i + x;
					if(x_idx < 0)
						x_idx += img->buffer_width();
					if(x_idx >= img->buffer_width())
						x_idx -= img->buffer_width();
					size_t index = y_idx * img->buffer_width() * 4
						+ x_idx * 4;
					size_t kern_index = (j+kern_height/2) * kern_width + (i+kern_width/2);
					uchar r = img->buffer()[index + 0];
					newpix[0] += kernel[kern_index] * r;
					uchar g = img->buffer()[index + 1];
					newpix[1] += kernel[kern_index] * g;
					uchar b = img->buffer()[index + 2];
					newpix[2] += kernel[kern_index] * b;
				}
			}
			uchar newpixel[4];
			newpixel[0] = (newpix[0] < 255) ? (newpix[0] > 0 ? uchar(newpix[0]) : 0) : 255;
			newpixel[1] = (newpix[1] < 255) ? (newpix[1] > 0 ? uchar(newpix[1]) : 0) : 255;
			newpixel[2] = (newpix[2] < 255) ? (newpix[2] > 0 ? uchar(newpix[2]) : 0) : 255;
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}
	return newimage;
}

void sharpen_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;

	double sharpen_kernel[9] = {
		0.1*(-1), 0.1*(-2), 0.1*(-1),
		0.1*(-2), 0.1*(22), 0.1*(-2),
		0.1*(-1), 0.1*(-2), 0.1*(-1)
	};
	Image* newimage = filter(sharpen_kernel, 3, 3, img);

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void blur_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	const char* sigma_str = input("Blur strength", "5.0");
	if(!sigma_str)
		return;
	for(int i = 0; i < strlen(sigma_str); i++)
		if(!isdigit(sigma_str[i]) && sigma_str[i] != '.')
			return;
	double sigma = atof(sigma_str);
	if(sigma == 0)
		return;

	double sigma2 = sigma * sigma;

	working = true;

	double blur_kernel[9];
	double sum = 0;
	for(int k = 0; k < 3; k++)
		for(int p = 0; p < 3; p++)
		{
			blur_kernel[k*3+p] = exp(double(k*k+p*p)/(-2*sigma2));
			sum += blur_kernel[k*3+p];
		}
	for(int i = 0; i < 9; i++)
		blur_kernel[i] /= sum;

	Image* newimage = filter(blur_kernel, 3, 3, img);

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void do_simple_grayscale()
{
	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setsize(img->buffer_width(), img->buffer_height());
	newimage->setpixeltype(RGB32);

	for(int y = 0; y < img->buffer_height(); y++)
	{
		bar->position(100.0 * (double) y / (img->buffer_height()));
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width(); x++)
		{
			size_t index = (y * img->buffer_width() + x) * 4;
			uchar r = img->buffer()[index + 0];
			uchar g = img->buffer()[index + 1];
			uchar b = img->buffer()[index + 2];

			double val = 0.299 * r + 0.587 * g + 0.114 * b;
			if(val > 255)
				val = 255;
			uchar newpixel[4];
			newpixel[0] = uchar(val);
			newpixel[1] = uchar(val);
			newpixel[2] = uchar(val);
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();
}

void edge_detection_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;

	double edgedet_kernel[9] = {
		0, -1, 0,
		-1, 4, -1,
		0, -1, 0
	};
	Image* newimage = filter(edgedet_kernel, 3, 3, img);

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;

	do_simple_grayscale();

	working = false;
}

void emboss_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;

	double emboss_kernel[9] = {
		0, 1, 0,
		1, 0, -1,
		0, -1, 0
	};
	Image* newimage = filter(emboss_kernel, 3, 3, img);

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;

	do_simple_grayscale();

	working = false;
}

void simple_grayscale_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;
	do_simple_grayscale();
	working = false;
}

Window* custom_filter_dialog = NULL;
ValueInput *factor, *a00, *a01, *a02, *a10, *a11, *a12, *a20, *a21, *a22;

void do_custom_cb(Widget*, void*)
{
	custom_filter_dialog->hide();
	working = true;

	double kernel[9];
	kernel[0] = a00->value() * factor->value();
	kernel[1] = a01->value() * factor->value();
	kernel[2] = a02->value() * factor->value();
	kernel[3] = a10->value() * factor->value();
	kernel[4] = a11->value() * factor->value();
	kernel[5] = a12->value() * factor->value();
	kernel[6] = a20->value() * factor->value();
	kernel[7] = a21->value() * factor->value();
	kernel[8] = a22->value() * factor->value();

	Image* newimage = filter(kernel, 3, 3, img);

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void custom_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	if(!custom_filter_dialog)
	{
		custom_filter_dialog = new Window(300, 200, "Custom filter coeffs");
		custom_filter_dialog->set_modal();
		custom_filter_dialog->begin();
		a00 = new ValueInput(75, 25, 50, 20);
		a01 = new ValueInput(150, 25, 50, 20);
		a02 = new ValueInput(225, 25, 50, 20);
		a10 = new ValueInput(75, 70, 50, 20);
		a11 = new ValueInput(150, 70, 50, 20);
		a12 = new ValueInput(225, 70, 50, 20);
		a20 = new ValueInput(75, 115, 50, 20);
		a21 = new ValueInput(150, 115, 50, 20);
		a22 = new ValueInput(225, 115, 50, 20);
		factor = new ValueInput(10, 70, 50, 20);
		factor->value(1.0);
		ReturnButton* b = new ReturnButton(200, 150, 75, 25, "OK");
		b->callback(do_custom_cb);
		custom_filter_dialog->end();
	}
	custom_filter_dialog->show();
}

void binarization_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setimage( img->buffer(),
		RGB32,
		img->buffer_width(),
		img->buffer_height() );

	for(int y = 0; y < img->buffer_height(); y++)
	{
		bar->position(100.0 * (double) y / img->buffer_height());
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width(); x++)
		{
			size_t index = y * img->buffer_width() * 4 + x * 4;
			uchar r = img->buffer()[index + 0];
			uchar g = img->buffer()[index + 1];
			uchar b = img->buffer()[index + 2];

			unsigned long sum = r + g + b;
			uchar newpixel[4], tag;
			if(sum > 255 * 3 / 2)
				tag = 255;
			else
				tag = 0;

			newpixel[0] = tag;
			newpixel[1] = tag;
			newpixel[2] = tag;
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void random_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setimage( img->buffer(),
		RGB32,
		img->buffer_width(),
		img->buffer_height() );

	for(int y = 0; y < img->buffer_height(); y++)
	{
		bar->position(100.0 * (double) y / img->buffer_height());
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width(); x++)
		{
			size_t index = y * img->buffer_width() * 4 + x * 4;
			uchar r = img->buffer()[index + 0];
			uchar g = img->buffer()[index + 1];
			uchar b = img->buffer()[index + 2];

			unsigned long sum = r + g + b;
			uchar newpixel[4], tag;
			if(sum > unsigned(rand() % (255 * 3)))
				tag = 255;
			else
				tag = 0;

			newpixel[0] = tag;
			newpixel[1] = tag;
			newpixel[2] = tag;
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void bayer_cb(Widget*, void*)
{
	if(!img)
		return;
	if(working)
		return;

	working = true;

	double map[4][4] = {{1, 9, 3, 11}, {13, 5, 15, 7}, {4, 12, 2, 10}, {16, 8, 14, 6}};
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++)
			map[i][j] *= (255 / 17);

	Image* newimage = new Image;
	img->forceARGB32();
	newimage->setimage( img->buffer(),
		RGB32,
		img->buffer_width(),
		img->buffer_height() );

	for(int y = 0; y < img->buffer_height(); y++)
	{
		bar->position(100.0 * (double) y / img->buffer_height());
		bar->redraw();
		fltk::wait(0);
		for(int x = 0; x < img->buffer_width(); x++)
		{
			size_t index = y * img->buffer_width() * 4 + x * 4;
			uchar r = img->buffer()[index + 0];
			uchar g = img->buffer()[index + 1];
			uchar b = img->buffer()[index + 2];

			unsigned long sum = (r + g + b) / 3;
			uchar newpixel[4], tag;
			if((sum + map[x % 4][y % 4]) > 255 * 2 / 2)
				tag = 255;
			else
				tag = 0;

			newpixel[0] = tag;
			newpixel[1] = tag;
			newpixel[2] = tag;
			newpixel[3] = 0;
			newimage->setpixels(&newpixel[0], Rectangle(x, y, 1, 1));
		}
	}

	images.push_back(img);
	newimage->buffer_changed();
	img = newimage;
	image_box->image(img);
	bar->position(0);
	image_box->redraw();

	working = false;
}

void fast_marching_cb(Widget*, void*)
{
	if(!img || !oldimg)
		return;

	images.push_back(oldimg);
	Image* i = inpaint_fast_marching(oldimg, mask);
	oldimg = NULL;
	images.push_back(img);
	img = i;
	image_box->image(img);
	image_box->redraw();
}

void criminisi_cb(Widget*, void*)
{
	if(!img || !oldimg)
		return;

	images.push_back(oldimg);
	Image* i = inpaint_criminisi(img, mask);
	oldimg = NULL;
	images.push_back(img);
	img = i;
	image_box->image(img);
	image_box->redraw();

}

static void build_menus(MenuBar* menu, Widget* w)
{
	ItemGroup* g;
	menu->user_data(w);
	menu->begin();
	g = new fltk::ItemGroup( "&File" );
	g->begin();
//	new Item( "&New File",        0, (Callback *)new_cb );
	new Item( "&Open File...",    COMMAND + 'o', (Callback*)open_cb );
	new Divider;
//	new Item( "&Save File",       COMMAND + 's', (Callback*)save_cb);
//	new Item( "Save File &As...", COMMAND + SHIFT + 's', (Callback *)saveas_cb);
	new Divider;
	new Item( "&Quit", COMMAND + 'q', (Callback*)quit_cb, 0 );
	g->end();
	g = new ItemGroup( "&Edit" );
	g->begin();
	new Item( "Undo", COMMAND + 'z', (Callback*)undo_cb );
	new Divider;
	new Item( "&Sliding average",  COMMAND + 's', (Callback*)sliding_avg_cb);
	new Divider;
	new Item( "Upscale &NN",      COMMAND + 'n', (Callback*)upscale_nn_cb);
	new Item( "Upscale &bilinear",COMMAND + 'b', (Callback*)upscale_bl_cb);
	new Divider;
	new Item( "Sha&rpen filter",COMMAND + 'r', (Callback*)sharpen_cb);
	new Item( "Bl&ur filter",COMMAND + 'u', (Callback*)blur_cb);
	new Item( "E&dge detection filter",COMMAND + 'd', (Callback*)edge_detection_cb);
	new Item( "&Emboss filter",COMMAND + 'e', (Callback*)emboss_cb);
	new Item( "&Custom filter",COMMAND + 'c', (Callback*)custom_cb);
	new Divider;
	new Item( "Grayscale s&imple", COMMAND + 'i', (Callback*)simple_grayscale_cb);
	new Divider;
	new Item( "&Binarization dithering", COMMAND + 'b', (Callback*)binarization_cb );
	new Item( "R&andom dithering", COMMAND + 'a', (Callback*)random_cb );
	new Item( "Ba&yer dithering", COMMAND + 'y', (Callback*)bayer_cb );
	new Divider;
	new Item( "&Fast marching inpaint", COMMAND + 'f', (Callback*)fast_marching_cb );
	new Item( "Cri&minisi inpaint", COMMAND + 'm', (Callback*)criminisi_cb );
	g->end();
	menu->end();
}


int main(int argc, char **argv)
{
	register_images();

	Window window(800, 650);
	window.begin();
	MenuBar menubar(0, 0, 600, 25);
	build_menus(&menubar, &window);
	DisplayWidget box(0, 30, 800, 600);
	bar = new ProgressBar(0, 630, 800, 15);
	image_box = &box;
	window.resizable(box);
	window.end();

	window.show(argc, argv);
	return run();
}

