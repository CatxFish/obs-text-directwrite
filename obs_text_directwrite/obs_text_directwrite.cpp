


#include <util/platform.h>
#include <util/util.hpp>
#include <obs-module.h>
#include <sys/stat.h>
#include <windows.h>
#include <algorithm>
#include <string>
#include <memory>
#include <math.h>    
#include "CustomTextRenderer.h"

using namespace std;

#ifndef clamp
#define clamp(val, min_val, max_val) \
	if (val < min_val) val = min_val; \
				else if (val > max_val) val = max_val;
#endif

#define MIN_SIZE_CX 2.0
#define MIN_SIZE_CY 2.0
#define MAX_SIZE_CX 4096.0
#define MAX_SIZE_CY 4096.0

/* ------------------------------------------------------------------------- */

#define S_FONT                          "font"
#define S_USE_FILE                      "read_from_file"
#define S_FILE                          "file"
#define S_TEXT                          "text"
#define S_COLOR                         "color"
#define S_GRADIENT                      "gradient"
#define S_GRADIENT_NONE                 "none"
#define S_GRADIENT_TWO                  "two_colors"
#define S_GRADIENT_THREE                "three_colors"
#define S_GRADIENT_FOUR                 "four_colors"
#define S_GRADIENT_COLOR                "gradient_color"
#define S_GRADIENT_COLOR2               "gradient_color2"
#define S_GRADIENT_COLOR3               "gradient_color3"
#define S_GRADIENT_DIR                  "gradient_dir"
#define S_GRADIENT_OPACITY              "gradient_opacity"
#define S_ALIGN                         "align"
#define S_VALIGN                        "valign"
#define S_OPACITY                       "opacity"
#define S_BKCOLOR                       "bk_color"
#define S_BKOPACITY                     "bk_opacity"
//#define S_VERTICAL                      "vertical"
#define S_OUTLINE                       "outline"
#define S_OUTLINE_SIZE                  "outline_size"
#define S_OUTLINE_COLOR                 "outline_color"
#define S_OUTLINE_OPACITY               "outline_opacity"
#define S_CHATLOG_MODE                  "chatlog"
#define S_CHATLOG_LINES                 "chatlog_lines"
#define S_EXTENTS                       "extents"
#define S_EXTENTS_CX                    "extents_cx"
#define S_EXTENTS_CY                    "extents_cy"

#define S_ALIGN_LEFT                    "left"
#define S_ALIGN_CENTER                  "center"
#define S_ALIGN_RIGHT                   "right"

#define S_VALIGN_TOP                    "top"
#define S_VALIGN_CENTER                 S_ALIGN_CENTER
#define S_VALIGN_BOTTOM                 "bottom"

#define T_(v)                           obs_module_text(v)
#define T_FONT                          T_("Font")
#define T_USE_FILE                      T_("ReadFromFile")
#define T_FILE                          T_("TextFile")
#define T_TEXT                          T_("Text")
#define T_COLOR                         T_("Color")
#define T_GRADIENT                      T_("Gradient")
#define T_GRADIENT_NONE                 T_("Gradient.None")
#define T_GRADIENT_TWO                  T_("Gradient.TwoColors")
#define T_GRADIENT_THREE                T_("Gradient.ThreeColors")
#define T_GRADIENT_FOUR                 T_("Gradient.FourColors")
#define T_GRADIENT_COLOR                T_("Gradient.Color")
#define T_GRADIENT_COLOR2               T_("Gradient.Color2")
#define T_GRADIENT_COLOR3               T_("Gradient.Color3")
#define T_GRADIENT_DIR                  T_("Gradient.Direction")
#define T_GRADIENT_OPACITY              T_("Gradient.Opacity")
#define T_ALIGN                         T_("Alignment")
#define T_VALIGN                        T_("VerticalAlignment")
#define T_OPACITY                       T_("Opacity")
#define T_BKCOLOR                       T_("BkColor")
#define T_BKOPACITY                     T_("BkOpacity")
//#define T_VERTICAL                      T_("Vertical")
#define T_OUTLINE                       T_("Outline")
#define T_OUTLINE_SIZE                  T_("Outline.Size")
#define T_OUTLINE_COLOR                 T_("Outline.Color")
#define T_OUTLINE_OPACITY               T_("Outline.Opacity")
#define T_CHATLOG_MODE                  T_("ChatlogMode")
#define T_CHATLOG_LINES                 T_("ChatlogMode.Lines")
#define T_EXTENTS                       T_("UseCustomExtents")
#define T_EXTENTS_CX                    T_("Width")
#define T_EXTENTS_CY                    T_("Height")

#define T_FILTER_TEXT_FILES             T_("Filter.TextFiles")
#define T_FILTER_ALL_FILES              T_("Filter.AllFiles")

#define T_ALIGN_LEFT                    T_("Alignment.Left")
#define T_ALIGN_CENTER                  T_("Alignment.Center")
#define T_ALIGN_RIGHT                   T_("Alignment.Right")

#define T_VALIGN_TOP                    T_("VerticalAlignment.Top")
#define T_VALIGN_CENTER                 T_ALIGN_CENTER
#define T_VALIGN_BOTTOM                 T_("VerticalAlignment.Bottom")

/* ------------------------------------------------------------------------- */

static inline DWORD get_alpha_val(uint32_t opacity)
{
	return ((opacity * 255 / 100) & 0xFF) << 24;
}

static inline DWORD calc_color(uint32_t color, uint32_t opacity)
{
	return color & 0xFFFFFF | get_alpha_val(opacity);
}

static inline wstring to_wide(const char *utf8)
{
	wstring text;

	size_t len = os_utf8_to_wcs(utf8, 0, nullptr, 0);
	text.resize(len);
	if (len)
		os_utf8_to_wcs(utf8, 0, &text[0], len + 1);

	return text;
}

static inline uint32_t rgb_to_bgr(uint32_t rgb)
{
	return ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16);
}

enum Gradient_mode
{
	Gradient_none = 0,
	Gradient_two_color = 2,
	Gradient_three_color = 3,
	Gradient_four_color = 4
};


struct TargetDC{

	void *bits;
	HDC memDC;
	HBITMAP hBmp;

	TargetDC(long width, long height)
	{
		BITMAPINFO info;
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -height;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biXPelsPerMeter = 0;
		info.bmiHeader.biYPelsPerMeter = 0;
		info.bmiHeader.biClrUsed = 0;
		info.bmiHeader.biClrImportant = 0;
		info.bmiHeader.biCompression = BI_RGB;
		info.bmiHeader.biSizeImage = width * height;
		info.bmiColors[0].rgbGreen = 0;
		info.bmiColors[0].rgbBlue = 0;
		info.bmiColors[0].rgbRed = 0;
		info.bmiColors[0].rgbReserved = 0;
		hBmp = CreateDIBSection(0, &info, DIB_RGB_COLORS, &bits, 0, 0);

		HDC hdc = GetDC(0);
		memDC = CreateCompatibleDC(hdc);
		SelectObject(memDC, hBmp);
		ReleaseDC(0, hdc);
	}

	~TargetDC()
	{
		DeleteObject(hBmp);
		DeleteDC(memDC);
	}
};

struct TextSource {

	obs_source_t *source = nullptr;
	gs_texture_t *tex = nullptr;

	uint32_t cx = 0;
	uint32_t cy = 0;

	IDWriteFactory* pDWriteFactory = nullptr;
	IDWriteTextFormat* pTextFormat = nullptr;
	ID2D1Factory* pD2DFactory = nullptr;
	ID2D1Brush* pFillBrush = nullptr;
	ID2D1Brush* pOutlineBrush = nullptr;
	ID2D1DCRenderTarget* pRT = nullptr;
	IDWriteTextLayout* pTextLayout = nullptr;
	CustomTextRenderer* pTextRenderer = nullptr;

	D2D1_RENDER_TARGET_PROPERTIES props = {};


	bool read_from_file = false;
	string file;
	time_t file_timestamp = 0;
	float update_time_elapsed = 0.0f;

	wstring text;
	wstring face;
	int face_size = 0;
	uint32_t color = 0xFFFFFF;
	uint32_t color2 = 0xFFFFFF;
	uint32_t color3 = 0xFFFFFF;
	uint32_t color4 = 0xFFFFFF;

	Gradient_mode gradient_count = Gradient_none;

	float gradient_dir = 0;
	float gradient_x = 0;
	float gradient_y = 0;
	float gradient2_x = 0;
	float gradient2_y = 0;

	uint32_t opacity = 100;
	uint32_t opacity2 = 100;
	uint32_t bk_color = 0;
	uint32_t bk_opacity = 0;

	DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR;
	DWRITE_FONT_STYLE  style = DWRITE_FONT_STYLE_NORMAL;
	DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
	DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING;
	DWRITE_PARAGRAPH_ALIGNMENT valign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;

	bool bold = false;
	bool italic = false;
	bool underline = false;
	bool strikeout = false;
	//bool vertical = false;

	bool use_outline = false;
	float outline_size = 0.0f;
	uint32_t outline_color = 0;
	uint32_t outline_opacity = 100;

	bool use_extents = false;
	uint32_t extents_cx = 0;
	uint32_t extents_cy = 0;

	bool chatlog_mode = false;
	int chatlog_lines = 6;

	/* --------------------------- */

	inline TextSource(obs_source_t *source_, obs_data_t *settings) :
		source(source_)
	{
		InitializeDirectWrite();
		obs_source_update(source, settings);
	}

	inline ~TextSource()
	{
		if (tex) {
			obs_enter_graphics();
			gs_texture_destroy(tex);
			obs_leave_graphics();
		}
		ReleaseResource();
	}

	void UpdateFont();
	void CalculateGradientAxis(float width, float height);
	void InitializeDirectWrite();
	void ReleaseResource();
	void UpdateBrush(ID2D1RenderTarget* pRT,
		ID2D1Brush** ppOutlineBrush,
		ID2D1Brush** ppFillBrush,
		float width,
		float height);
	void RenderText();
	void LoadFileText();


	const char *GetMainString(const char *str);

	inline void Update(obs_data_t *settings);
	inline void Tick(float seconds);
	inline void Render(gs_effect_t *effect);
};

static time_t get_modified_timestamp(const char *filename)
{
	struct stat stats;
	if (os_stat(filename, &stats) != 0)
		return -1;
	return stats.st_mtime;
}

void TextSource::UpdateFont()
{

	if (bold)
		weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_BOLD;
	else
		weight = DWRITE_FONT_WEIGHT::DWRITE_FONT_WEIGHT_REGULAR;

	if (italic)
		style = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_ITALIC;
	else
		style = DWRITE_FONT_STYLE::DWRITE_FONT_STYLE_NORMAL;

}


void TextSource::CalculateGradientAxis(float width, float height)
{
	if (width <= 0.0 || height <= 0.0)
		return;

	float angle = atan(height / width)*180.0 / M_PI;

	if (gradient_dir <= angle || gradient_dir > 360.0 - angle)
	{
		float y = width / 2 * tan(gradient_dir*M_PI / 180.0);
		gradient_x = width;
		gradient_y = height / 2 - y;
		gradient2_x = 0;
		gradient2_y = height / 2 + y;

	}
	else if (gradient_dir <= 180.0 - angle && gradient_dir> angle)
	{
		float x = height / 2 * tan((90.0 - gradient_dir)*M_PI / 180.0);
		gradient_x = width / 2 + x;
		gradient_y = 0;
		gradient2_x = width / 2 - x;
		gradient2_y = height;
	}
	else if (gradient_dir <= 180.0 + angle && gradient_dir > 180.0 - angle)
	{
		float y = width / 2 * tan(gradient_dir*M_PI / 180.0);
		gradient_x = 0;
		gradient_y = height / 2 + y;
		gradient2_x = width;
		gradient2_y = height / 2 - y;
	}
	else
	{
		float x = height / 2 * tan((270.0 - gradient_dir)*M_PI / 180.0);
		gradient_x = width / 2 - x;
		gradient_y = height;
		gradient2_x = width / 2 + x;
		gradient2_y = 0;
	}

}

void TextSource::InitializeDirectWrite()
{
	HRESULT hr = S_OK;

	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);

	if (SUCCEEDED(hr))
	{
		hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&pDWriteFactory)
			);
	}


	props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED),
		0,
		0,
		D2D1_RENDER_TARGET_USAGE_NONE,
		D2D1_FEATURE_LEVEL_DEFAULT
		);

}

void TextSource::ReleaseResource()
{


	SafeRelease(&pTextRenderer);
	SafeRelease(&pRT);
	SafeRelease(&pFillBrush);
	SafeRelease(&pOutlineBrush);
	SafeRelease(&pTextLayout);
	SafeRelease(&pTextFormat);
	SafeRelease(&pDWriteFactory);
	SafeRelease(&pD2DFactory);


}


void TextSource::UpdateBrush(ID2D1RenderTarget* pRT, ID2D1Brush**
	ppOutlineBrush, ID2D1Brush** ppFillBrush
	, float width, float height)
{
	HRESULT hr;

	if (gradient_count > 0)
	{
		CalculateGradientAxis(width, height);

		ID2D1GradientStopCollection *pGradientStops = NULL;

		float level = 1.0 / (gradient_count - 1);

		D2D1_GRADIENT_STOP gradientStops[4];
		gradientStops[0].color = D2D1::ColorF(color, opacity / 100.0);
		gradientStops[0].position = 0.0f;
		gradientStops[1].color = D2D1::ColorF(color2, opacity2 / 100.0);
		gradientStops[1].position = gradientStops[0].position + level;
		gradientStops[2].color = D2D1::ColorF(color3, opacity2 / 100.0);
		gradientStops[2].position = gradientStops[1].position + level;
		gradientStops[3].color = D2D1::ColorF(color4, opacity2 / 100.0);
		gradientStops[3].position = 1.0f;

		hr = pRT->CreateGradientStopCollection(gradientStops, gradient_count,
			D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_MIRROR, &pGradientStops);

		hr = pRT->CreateLinearGradientBrush(
			D2D1::LinearGradientBrushProperties(
			D2D1::Point2F(gradient_x, gradient_y),
			D2D1::Point2F(gradient2_x, gradient2_y)),
			pGradientStops,
			(ID2D1LinearGradientBrush**)ppFillBrush
			);

	}
	else
	{
		hr = pRT->CreateSolidColorBrush(D2D1::ColorF(color, opacity / 100.0),
			(ID2D1SolidColorBrush**)ppFillBrush
			);
	}

	if (use_outline)
	{
		hr = pRT->CreateSolidColorBrush(
			D2D1::ColorF(outline_color, outline_opacity / 100.0),
			(ID2D1SolidColorBrush**)ppOutlineBrush
			);
	}

}



void TextSource::RenderText()
{

	UINT32 TextLength = (UINT32)wcslen(text.c_str());
	HRESULT hr = S_OK;

	float layout_cx = (use_extents) ? extents_cx : 1920.0;
	float layout_cy = (use_extents) ? extents_cy : 1080.0;
	float text_cx = 0.0;
	float text_cy = 0.0;

	SIZE size;
	UINT32 lines = 1;

	if (pDWriteFactory)
	{
		hr = pDWriteFactory->CreateTextFormat(face.c_str(), NULL, weight,
			style, stretch, (float)face_size, L"en-us", &pTextFormat);
	}


	if (SUCCEEDED(hr))
	{
		pTextFormat->SetTextAlignment(align);
		pTextFormat->SetParagraphAlignment(valign);
		pTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

		/*if (vertical)
		{
		pTextFormat->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
		pTextFormat->SetFlowDirection(DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
		}*/

		hr = pDWriteFactory->CreateTextLayout(text.c_str(), TextLength,
			pTextFormat, layout_cx, layout_cy, &pTextLayout);
	}


	if (SUCCEEDED(hr))
	{
		DWRITE_TEXT_METRICS textMetrics;
		hr = pTextLayout->GetMetrics(&textMetrics);

		text_cx = ceil(textMetrics.widthIncludingTrailingWhitespace);
		text_cy = ceil(textMetrics.height);
		lines = textMetrics.lineCount;

		if (!use_extents)
		{
			layout_cx = text_cx;
			layout_cy = text_cy;
		}

		clamp(layout_cx, MIN_SIZE_CX, MAX_SIZE_CX);
		clamp(layout_cy, MIN_SIZE_CY, MAX_SIZE_CY);

		size.cx = layout_cx;
		size.cy = layout_cy;
	}

	if (SUCCEEDED(hr))
	{

		DWRITE_TEXT_RANGE text_range = { 0, TextLength };
		pTextLayout->SetUnderline(underline, text_range);
		pTextLayout->SetStrikethrough(strikeout, text_range);
		pTextLayout->SetMaxWidth(layout_cx);
		pTextLayout->SetMaxHeight(layout_cy);
		hr = pD2DFactory->CreateDCRenderTarget(&props, &pRT);
	}


	TargetDC target(size.cx, size.cy);

	if (SUCCEEDED(hr))
	{

		RECT rc;
		SetRect(&rc, 0, 0, size.cx, size.cy);
		pRT->BindDC(target.memDC, &rc);

		UpdateBrush(pRT, &pOutlineBrush, &pFillBrush, text_cx, text_cy / lines);

		pTextRenderer = new (std::nothrow)CustomTextRenderer(
			pD2DFactory,
			(ID2D1RenderTarget*)pRT,
			(ID2D1Brush*)pOutlineBrush,
			(ID2D1Brush*)pFillBrush,
			outline_size
			);

		pRT->BeginDraw();

		pRT->SetTransform(D2D1::IdentityMatrix());

		pRT->Clear(D2D1::ColorF(bk_color, bk_opacity / 100.0));

		pTextLayout->Draw(NULL, pTextRenderer, 0, 0);

		hr = pRT->EndDraw();
	}

	if (!tex || (LONG)cx != size.cx || (LONG)cy != size.cy) {
		obs_enter_graphics();
		if (tex)
			gs_texture_destroy(tex);

		const uint8_t *data = (uint8_t*)target.bits;
		tex = gs_texture_create(size.cx, size.cy, GS_BGRA, 1, &data,
			GS_DYNAMIC);

		obs_leave_graphics();

		cx = (uint32_t)size.cx;
		cy = (uint32_t)size.cy;

	}
	else if (tex) {
		obs_enter_graphics();
		const uint8_t *data = (uint8_t*)target.bits;
		gs_texture_set_image(tex, data, size.cx * 4, false);
		obs_leave_graphics();
	}



	SafeRelease(&pTextFormat);
	SafeRelease(&pFillBrush);
	SafeRelease(&pOutlineBrush);
	SafeRelease(&pTextLayout);
	SafeRelease(&pTextRenderer);
	SafeRelease(&pRT);

}

const char *TextSource::GetMainString(const char *str)
{
	if (!str)
		return "";
	if (!chatlog_mode || !chatlog_lines)
		return str;

	int lines = chatlog_lines;
	size_t len = strlen(str);
	if (!len)
		return str;

	const char *temp = str + len;

	while (temp != str) {
		temp--;

		if (temp[0] == '\n' && temp[1] != 0) {
			if (!--lines)
				break;
		}
	}

	return *temp == '\n' ? temp + 1 : temp;
}

void TextSource::LoadFileText()
{
	BPtr<char> file_text = os_quick_read_utf8_file(file.c_str());
	text = to_wide(GetMainString(file_text));
}

#define obs_data_get_uint32 (uint32_t)obs_data_get_int

inline void TextSource::Update(obs_data_t *s)
{
	const char *new_text = obs_data_get_string(s, S_TEXT);
	obs_data_t *font_obj = obs_data_get_obj(s, S_FONT);
	const char *align_str = obs_data_get_string(s, S_ALIGN);
	const char *valign_str = obs_data_get_string(s, S_VALIGN);
	uint32_t new_color = obs_data_get_uint32(s, S_COLOR);
	uint32_t new_opacity = obs_data_get_uint32(s, S_OPACITY);
	const char *gradient_str = obs_data_get_string(s, S_GRADIENT);
	uint32_t new_color2 = obs_data_get_uint32(s, S_GRADIENT_COLOR);
	uint32_t new_color3 = obs_data_get_uint32(s, S_GRADIENT_COLOR2);
	uint32_t new_color4 = obs_data_get_uint32(s, S_GRADIENT_COLOR3);
	uint32_t new_opacity2 = obs_data_get_uint32(s, S_GRADIENT_OPACITY);
	float new_grad_dir = (float)obs_data_get_double(s, S_GRADIENT_DIR);
	//bool new_vertical = obs_data_get_bool(s, S_VERTICAL);
	bool new_outline = obs_data_get_bool(s, S_OUTLINE);
	uint32_t new_o_color = obs_data_get_uint32(s, S_OUTLINE_COLOR);
	uint32_t new_o_opacity = obs_data_get_uint32(s, S_OUTLINE_OPACITY);
	uint32_t new_o_size = obs_data_get_uint32(s, S_OUTLINE_SIZE);
	bool new_use_file = obs_data_get_bool(s, S_USE_FILE);
	const char *new_file = obs_data_get_string(s, S_FILE);
	bool new_chat_mode = obs_data_get_bool(s, S_CHATLOG_MODE);
	int new_chat_lines = (int)obs_data_get_int(s, S_CHATLOG_LINES);
	bool new_extents = obs_data_get_bool(s, S_EXTENTS);
	uint32_t n_extents_cx = obs_data_get_uint32(s, S_EXTENTS_CX);
	uint32_t n_extents_cy = obs_data_get_uint32(s, S_EXTENTS_CY);

	const char *font_face = obs_data_get_string(font_obj, "face");
	int font_size = (int)obs_data_get_int(font_obj, "size");
	int64_t font_flags = obs_data_get_int(font_obj, "flags");
	bool new_bold = (font_flags & OBS_FONT_BOLD) != 0;
	bool new_italic = (font_flags & OBS_FONT_ITALIC) != 0;
	bool new_underline = (font_flags & OBS_FONT_UNDERLINE) != 0;
	bool new_strikeout = (font_flags & OBS_FONT_STRIKEOUT) != 0;

	uint32_t new_bk_color = obs_data_get_uint32(s, S_BKCOLOR);
	uint32_t new_bk_opacity = obs_data_get_uint32(s, S_BKOPACITY);

	/* ----------------------------- */

	wstring new_face = to_wide(font_face);

	if (new_face != face ||
		face_size != font_size ||
		new_bold != bold ||
		new_italic != italic ||
		new_underline != underline ||
		new_strikeout != strikeout) {

		face = new_face;
		face_size = font_size;
		bold = new_bold;
		italic = new_italic;
		underline = new_underline;
		strikeout = new_strikeout;

		UpdateFont();
	}

	/* ----------------------------- */

	new_color = rgb_to_bgr(new_color);
	new_color2 = rgb_to_bgr(new_color2);
	new_color3 = rgb_to_bgr(new_color3);
	new_color4 = rgb_to_bgr(new_color4);
	new_o_color = rgb_to_bgr(new_o_color);
	new_bk_color = rgb_to_bgr(new_bk_color);

	color = new_color;
	opacity = new_opacity;
	color2 = new_color2;
	color3 = new_color3;
	color4 = new_color4;
	opacity2 = new_opacity2;
	gradient_dir = new_grad_dir;
	//vertical = new_vertical;

	Gradient_mode new_count = Gradient_none;

	if (strcmp(gradient_str, S_GRADIENT_NONE) == 0)
		new_count = Gradient_none;
	else if (strcmp(gradient_str, S_GRADIENT_TWO) == 0)
		new_count = Gradient_two_color;
	else if (strcmp(gradient_str, S_GRADIENT_THREE) == 0)
		new_count = Gradient_three_color;
	else
		new_count = Gradient_four_color;

	gradient_count = new_count;

	bk_color = new_bk_color;
	bk_opacity = new_bk_opacity;
	use_extents = new_extents;
	extents_cx = n_extents_cx;
	extents_cy = n_extents_cy;

	read_from_file = new_use_file;

	chatlog_mode = new_chat_mode;
	chatlog_lines = new_chat_lines;

	if (read_from_file) {
		file = new_file;
		file_timestamp = get_modified_timestamp(new_file);
		LoadFileText();

	}
	else {
		text = to_wide(GetMainString(new_text));

	}

	use_outline = new_outline;
	outline_color = new_o_color;
	outline_opacity = new_o_opacity;
	outline_size = roundf(float(new_o_size));

	if (strcmp(align_str, S_ALIGN_CENTER) == 0)
		align = DWRITE_TEXT_ALIGNMENT_CENTER;
	else if (strcmp(align_str, S_ALIGN_RIGHT) == 0)
		align = DWRITE_TEXT_ALIGNMENT_TRAILING;
	else
		align = DWRITE_TEXT_ALIGNMENT_LEADING;

	if (strcmp(valign_str, S_VALIGN_CENTER) == 0)
		valign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
	else if (strcmp(valign_str, S_VALIGN_BOTTOM) == 0)
		valign = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
	else
		valign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;

	RenderText();
	update_time_elapsed = 0.0f;

	/* ----------------------------- */

	obs_data_release(font_obj);
}

inline void TextSource::Tick(float seconds)
{
	if (!read_from_file)
		return;

	update_time_elapsed += seconds;

	if (update_time_elapsed >= 1.0f) {
		time_t t = get_modified_timestamp(file.c_str());
		update_time_elapsed = 0.0f;

		if (file_timestamp != t) {
			LoadFileText();
			RenderText();
			file_timestamp = t;
		}
	}
}

inline void TextSource::Render(gs_effect_t *effect)
{
	if (!tex)
		return;

	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), tex);
	gs_draw_sprite(tex, 0, cx, cy);
}

/* ------------------------------------------------------------------------- */

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-text", "en-US")

#define set_vis(var, val, show) \
	do { \
		p = obs_properties_get(props, val); \
		obs_property_set_visible(p, var == show); \
			} while (false)

static bool use_file_changed(obs_properties_t *props, obs_property_t *p,
obs_data_t *s)
{
	bool use_file = obs_data_get_bool(s, S_USE_FILE);

	set_vis(use_file, S_TEXT, false);
	set_vis(use_file, S_FILE, true);
	return true;
}

static bool outline_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	bool outline = obs_data_get_bool(s, S_OUTLINE);

	set_vis(outline, S_OUTLINE_SIZE, true);
	set_vis(outline, S_OUTLINE_COLOR, true);
	set_vis(outline, S_OUTLINE_OPACITY, true);
	return true;
}

static bool chatlog_mode_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	bool chatlog_mode = obs_data_get_bool(s, S_CHATLOG_MODE);

	set_vis(chatlog_mode, S_CHATLOG_LINES, true);
	return true;
}

static bool gradient_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	const char *gradient_str = obs_data_get_string(s, S_GRADIENT);
	Gradient_mode mode = Gradient_none;

	if (strcmp(gradient_str, S_GRADIENT_NONE) == 0)
		mode = Gradient_none;
	else if (strcmp(gradient_str, S_GRADIENT_TWO) == 0)
		mode = Gradient_two_color;
	else if (strcmp(gradient_str, S_GRADIENT_THREE) == 0)
		mode = Gradient_three_color;
	else
		mode = Gradient_four_color;

	bool gradient_color = (mode > Gradient_none);
	bool gradient_color2 = (mode > Gradient_two_color);
	bool gradient_color3 = (mode > Gradient_three_color);


	set_vis(gradient_color, S_GRADIENT_COLOR, true);
	set_vis(gradient_color2, S_GRADIENT_COLOR2, true);
	set_vis(gradient_color3, S_GRADIENT_COLOR3, true);
	set_vis(gradient_color, S_GRADIENT_OPACITY, true);
	set_vis(gradient_color, S_GRADIENT_DIR, true);

	return true;
}

static bool extents_modified(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	bool use_extents = obs_data_get_bool(s, S_EXTENTS);

	set_vis(use_extents, S_EXTENTS_CX, true);
	set_vis(use_extents, S_EXTENTS_CY, true);
	return true;
}

#undef set_vis

static obs_properties_t *get_properties(void *data)
{
	TextSource *s = reinterpret_cast<TextSource*>(data);
	string path;

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_properties_add_font(props, S_FONT, T_FONT);

	p = obs_properties_add_bool(props, S_USE_FILE, T_USE_FILE);
	obs_property_set_modified_callback(p, use_file_changed);

	string filter;
	filter += T_FILTER_TEXT_FILES;
	filter += " (*.txt);;";
	filter += T_FILTER_ALL_FILES;
	filter += " (*.*)";

	if (s && !s->file.empty()) {
		const char *slash;

		path = s->file;
		replace(path.begin(), path.end(), '\\', '/');
		slash = strrchr(path.c_str(), '/');
		if (slash)
			path.resize(slash - path.c_str() + 1);
	}

	obs_properties_add_text(props, S_TEXT, T_TEXT, OBS_TEXT_MULTILINE);
	obs_properties_add_path(props, S_FILE, T_FILE, OBS_PATH_FILE,
		filter.c_str(), path.c_str());

	//obs_properties_add_bool(props, S_VERTICAL, T_VERTICAL);
	obs_properties_add_color(props, S_COLOR, T_COLOR);
	obs_properties_add_int_slider(props, S_OPACITY, T_OPACITY, 0, 100, 1);

	/*p = obs_properties_add_bool(props, S_GRADIENT, T_GRADIENT);
	obs_property_set_modified_callback(p, gradient_changed);*/

	p = obs_properties_add_list(props, S_GRADIENT, T_GRADIENT,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p, T_GRADIENT_NONE, S_GRADIENT_NONE);
	obs_property_list_add_string(p, T_GRADIENT_TWO, S_GRADIENT_TWO);
	obs_property_list_add_string(p, T_GRADIENT_THREE, S_GRADIENT_THREE);
	obs_property_list_add_string(p, T_GRADIENT_FOUR, S_GRADIENT_FOUR);

	obs_property_set_modified_callback(p, gradient_changed);

	obs_properties_add_color(props, S_GRADIENT_COLOR, T_GRADIENT_COLOR);
	obs_properties_add_color(props, S_GRADIENT_COLOR2, T_GRADIENT_COLOR2);
	obs_properties_add_color(props, S_GRADIENT_COLOR3, T_GRADIENT_COLOR3);
	obs_properties_add_int_slider(props, S_GRADIENT_OPACITY,
		T_GRADIENT_OPACITY, 0, 100, 1);
	obs_properties_add_float_slider(props, S_GRADIENT_DIR,
		T_GRADIENT_DIR, 0, 360, 0.1);

	obs_properties_add_color(props, S_BKCOLOR, T_BKCOLOR);
	obs_properties_add_int_slider(props, S_BKOPACITY, T_BKOPACITY,
		0, 100, 1);

	p = obs_properties_add_list(props, S_ALIGN, T_ALIGN,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_ALIGN_LEFT, S_ALIGN_LEFT);
	obs_property_list_add_string(p, T_ALIGN_CENTER, S_ALIGN_CENTER);
	obs_property_list_add_string(p, T_ALIGN_RIGHT, S_ALIGN_RIGHT);

	p = obs_properties_add_list(props, S_VALIGN, T_VALIGN,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_VALIGN_TOP, S_VALIGN_TOP);
	obs_property_list_add_string(p, T_VALIGN_CENTER, S_VALIGN_CENTER);
	obs_property_list_add_string(p, T_VALIGN_BOTTOM, S_VALIGN_BOTTOM);

	p = obs_properties_add_bool(props, S_OUTLINE, T_OUTLINE);
	obs_property_set_modified_callback(p, outline_changed);

	obs_properties_add_int(props, S_OUTLINE_SIZE, T_OUTLINE_SIZE, 1, 20, 1);
	obs_properties_add_color(props, S_OUTLINE_COLOR, T_OUTLINE_COLOR);
	obs_properties_add_int_slider(props, S_OUTLINE_OPACITY,
		T_OUTLINE_OPACITY, 0, 100, 1);

	p = obs_properties_add_bool(props, S_CHATLOG_MODE, T_CHATLOG_MODE);
	obs_property_set_modified_callback(p, chatlog_mode_changed);

	obs_properties_add_int(props, S_CHATLOG_LINES, T_CHATLOG_LINES,
		1, 1000, 1);

	p = obs_properties_add_bool(props, S_EXTENTS, T_EXTENTS);
	obs_property_set_modified_callback(p, extents_modified);

	obs_properties_add_int(props, S_EXTENTS_CX, T_EXTENTS_CX, 32, 8000, 1);
	obs_properties_add_int(props, S_EXTENTS_CY, T_EXTENTS_CY, 32, 8000, 1);

	return props;
}

bool obs_module_load(void)
{
	obs_source_info si = {};
	si.id = "text_directwrite";
	si.type = OBS_SOURCE_TYPE_INPUT;
	si.output_flags = OBS_SOURCE_VIDEO;
	si.get_properties = get_properties;

	si.get_name = [](void*)
	{
		return obs_module_text("TextDirectWrite");
	};
	si.create = [](obs_data_t *settings, obs_source_t *source)
	{
		return (void*)new TextSource(source, settings);
	};
	si.destroy = [](void *data)
	{
		delete reinterpret_cast<TextSource*>(data);
	};
	si.get_width = [](void *data)
	{
		return reinterpret_cast<TextSource*>(data)->cx;
	};
	si.get_height = [](void *data)
	{
		return reinterpret_cast<TextSource*>(data)->cy;
	};
	si.get_defaults = [](obs_data_t *settings)
	{
		obs_data_t *font_obj = obs_data_create();
		obs_data_set_default_string(font_obj, "face", "Arial");
		obs_data_set_default_int(font_obj, "size", 36);

		obs_data_set_default_obj(settings, S_FONT, font_obj);
		obs_data_set_default_string(settings, S_ALIGN, S_ALIGN_LEFT);
		obs_data_set_default_string(settings, S_VALIGN, S_VALIGN_TOP);
		obs_data_set_default_int(settings, S_COLOR, 0xFFFFFF);
		obs_data_set_default_int(settings, S_OPACITY, 100);
		obs_data_set_default_int(settings, S_GRADIENT_COLOR, 0xFFFFFF);
		obs_data_set_default_int(settings, S_GRADIENT_COLOR2, 0xFFFFFF);
		obs_data_set_default_int(settings, S_GRADIENT_COLOR3, 0xFFFFFF);
		obs_data_set_default_int(settings, S_GRADIENT_OPACITY, 100);
		obs_data_set_default_double(settings, S_GRADIENT_DIR, 90.0);
		obs_data_set_default_int(settings, S_BKCOLOR, 0x000000);
		obs_data_set_default_int(settings, S_BKOPACITY, 0);
		obs_data_set_default_int(settings, S_OUTLINE_SIZE, 2);
		obs_data_set_default_int(settings, S_OUTLINE_COLOR, 0xFFFFFF);
		obs_data_set_default_int(settings, S_OUTLINE_OPACITY, 100);
		obs_data_set_default_int(settings, S_CHATLOG_LINES, 6);
		obs_data_set_default_int(settings, S_EXTENTS_CX, 100);
		obs_data_set_default_int(settings, S_EXTENTS_CY, 100);

		obs_data_release(font_obj);
	};
	si.update = [](void *data, obs_data_t *settings)
	{
		reinterpret_cast<TextSource*>(data)->Update(settings);
	};
	si.video_tick = [](void *data, float seconds)
	{
		reinterpret_cast<TextSource*>(data)->Tick(seconds);
	};
	si.video_render = [](void *data, gs_effect_t *effect)
	{
		reinterpret_cast<TextSource*>(data)->Render(effect);
	};

	obs_register_source(&si);

	return true;
}

void obs_module_unload(void)
{
}

