#include "stdafx.h"
#include "glDraw.h"
#include <fstream>
#include <chrono>
#include <iostream>
#include <vector>
#include <png.h>
#include <thread>
#include <tuple>
#include "SafeQueue.h"
#include "WorkingDirectoryHeader.h"


int frame = 0;
auto start = std::chrono::high_resolution_clock::now();
auto num_frames = 10;
auto frame_time = 1000 / num_frames;
const std::string base_path = get_current_dir();
SafeQueue<std::tuple<std::vector<unsigned char>, int, int>> queue;
bool started = false;

bool write_png_file(const std::string& filename, const std::vector<unsigned char>& pixels, int width, int height, int channels) {
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png)
		return false;

	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_write_struct(&png, &info);
		return false;
	}

	FILE* fp = fopen(filename.c_str(), "wb");
	if (!fp) {
		png_destroy_write_struct(&png, &info);
		return false;
	}

	png_init_io(png, fp);
	png_set_IHDR(png, info, width, height, 8 /* depth */, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_colorp palette = (png_colorp)png_malloc(png, PNG_MAX_PALETTE_LENGTH * sizeof(png_color));
	if (!palette) {
		fclose(fp);
		png_destroy_write_struct(&png, &info);
		return false;
	}
	png_set_PLTE(png, info, palette, PNG_MAX_PALETTE_LENGTH);
	png_write_info(png, info);
	png_set_packing(png);

	png_bytepp rows = (png_bytepp)png_malloc(png, height * sizeof(png_bytep));
	for (int i = 0; i < height; ++i)
		rows[i] = (png_bytep)(&pixels[0] + (height - i - 1) * width * channels);

	png_write_image(png, rows);
	png_write_end(png, info);
	png_free(png, palette);
	png_destroy_write_struct(&png, &info);

	fclose(fp);
	delete[] rows;
	return true;
}

void convert_opengl_image_to_libpng(std::vector<unsigned char>& pixels, int width, int height, int channels) {
	// invert pixels (stolen from SOILs source code)
	for (int j = 0; j * 2 < height; ++j) {
		int x = j * width * 3;
		int y = (height - 1 - j) * width * 3;
		for (int i = width * 3; i > 0; --i) {
			uint8_t tmp = pixels[x];
			pixels[x] = pixels[y];
			pixels[y] = tmp;
			++x;
			++y;
		}
	}
}

void thread_loop() {
	while (true) {
		auto image = queue.dequeue();
		std::stringstream ss;
		ss << base_path << '\\';
		ss << "frame_" << frame++ << ".png";
		std::cout << ss.str() << std::endl;
		auto pixels_vector = std::get<0>(image);
		int width = std::get<1>(image);
		int height = std::get<2>(image);
		convert_opengl_image_to_libpng(pixels_vector, width, height, 3);
	
		write_png_file(ss.str(), pixels_vector, width, height, 3);
		pixels_vector.clear();
	}
}

void GL::Hook(char* function, uintptr_t& oFunction, void* hFunction)
{
	HMODULE hMod = GetModuleHandle("opengl32.dll");

	if (hMod)
	{
		oFunction = (uintptr_t)mem::TrampolineHook((void*)GetProcAddress(hMod, function), hFunction, 5);
	}
}

void GL::SetupOrtho()
{
	if (!started) {
		std::thread t(thread_loop);
		t.detach();
		started = true;
	}

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	glViewport(0, 0, viewport[2], viewport[3]);
	int width = viewport[2];
	int heigth = viewport[3];
	unsigned int buffer_size = 3 * width * heigth;
	auto current = std::chrono::high_resolution_clock::now();
	auto elapsed_miilisecond = std::chrono::duration_cast<std::chrono::milliseconds>(current - start).count();
	if (elapsed_miilisecond > frame_time) {
		std::vector<unsigned char> pixels_vector(buffer_size, 0);
		glReadPixels(0, 0, viewport[2], viewport[3], GL_RGB, GL_UNSIGNED_BYTE, &pixels_vector[0]);
		
		queue.enqueue(std::make_tuple(std::move(pixels_vector), width, heigth));
		start = std::chrono::high_resolution_clock::now();
	}
}

void GL::RestoreGL()
{
	//glEnable(GL_DEPTH_TEST);
}

void GL::DrawFilledRect(float x, float y, float width, float height, const GLubyte color[3])
{
	glColor3ub(color[0], color[1], color[2]);
	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x + width, y);
	glVertex2f(x + width, y + height);
	glVertex2f(x, y + height);
	glEnd();
}

void GL::DrawOutline(float x, float y, float width, float height, float lineWidth, const GLubyte color[3])
{
	glLineWidth(lineWidth);
	glBegin(GL_LINE_STRIP);
	glColor3ub(color[0], color[1], color[2]);
	glVertex2f(x - 0.5f, y - 0.5f);
	glVertex2f(x + width + 0.5f, y - 0.5f);
	glVertex2f(x + width + 0.5f, y + height + 0.5f);
	glVertex2f(x - 0.5f, y + height + 0.5f);
	glVertex2f(x - 0.5f, y - 0.5f);
	glEnd();
}