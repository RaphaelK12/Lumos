#include "JM.h"
#include "DebugRenderer.h"
#include "Graphics/API/Textures/Texture2D.h"
#include "App/Window.h"
#include "Graphics/API/Renderer.h"
#include "Graphics/Material.h"
#include "Graphics/API/VertexArray.h"

#include "Maths/MathsUtilities.h"
#include "Maths/BoundingBox.h"
#include "Maths/BoundingSphere.h"

#include <cstdarg>
#include <cstdio>

namespace jm
{
	maths::Vector3	DebugRenderer::m_CameraPosition;
	maths::Matrix4	DebugRenderer::m_ProjMtx;
	maths::Matrix4	DebugRenderer::m_ViewMtx;
	maths::Matrix4	DebugRenderer::m_ProjViewMtx;

	int DebugRenderer::m_NumStatusEntries = 0;
	float DebugRenderer::m_MaxStatusEntryWidth = 0.0f;
	std::vector<LogEntry> DebugRenderer::m_vLogEntries;
	int DebugRenderer::m_LogEntriesOffset = 0;
	size_t	DebugRenderer::m_OffsetChars = 0;

	std::vector<maths::Vector4> DebugRenderer::m_vChars;
	DebugRenderer::DebugDrawList DebugRenderer::m_DrawList;
	DebugRenderer::DebugDrawList DebugRenderer::m_DrawListNDT;

	Material* DebugRenderer::m_pShaderPoints = nullptr;
	Material* DebugRenderer::m_pShaderLines = nullptr;
	Material* DebugRenderer::m_pShaderHairLines = nullptr;
	Material* DebugRenderer::m_pShaderText = nullptr;

	VertexArray* DebugRenderer::m_VertexArray = nullptr;

	Texture2D* DebugRenderer::m_FontTex = nullptr;

	uint DebugRenderer::m_Width = 0;
	uint DebugRenderer::m_Height = 0;

#ifdef JM_PLATFORM_WINDOWS
#define VSNPRINTF( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList ) vsnprintf_s( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList )
#elif JM_PLATFORM_MACOS
#define VSNPRINTF( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList ) vsnprintf_l( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList )
#elif JM_PLATFORM_LINUX
#define VSNPRINTF( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList ) vsnprintf( _DstBuf, _DstSize, _Format, _ArgList )
#elif JM_PLATFORM_MOBILE
#define VSNPRINTF( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList ) 0
#else
#define VSNPRINTF( _DstBuf, _DstSize, _MaxCount, _Format, _ArgList ) 0
#endif

#ifndef JM_PLATFORM_WINDOWS
#define _TRUNCATE 0
#endif

	//Draw Point (circle)
	void DebugRenderer::GenDrawPoint(bool ndt, const maths::Vector3& pos, float point_radius, const maths::Vector4& colour)
	{
		auto list = ndt ? &m_DrawListNDT : &m_DrawList;
		list->_vPoints.emplace_back(pos, point_radius);
		list->_vPoints.push_back(colour);
	}

	void DebugRenderer::DrawPoint(const maths::Vector3& pos, float point_radius, const maths::Vector3& colour)
	{
		GenDrawPoint(false, pos, point_radius, maths::Vector4(colour, 1.0f));
	}
	void DebugRenderer::DrawPoint(const maths::Vector3& pos, float point_radius, const maths::Vector4& colour)
	{
		GenDrawPoint(false, pos, point_radius, colour);
	}
	void DebugRenderer::DrawPointNDT(const maths::Vector3& pos, float point_radius, const maths::Vector3& colour)
	{
		GenDrawPoint(true, pos, point_radius, maths::Vector4(colour, 1.0f));
	}
	void DebugRenderer::DrawPointNDT(const maths::Vector3& pos, float point_radius, const maths::Vector4& colour)
	{
		GenDrawPoint(true, pos, point_radius, colour);
	}

	//Draw Line with a given thickness
	void DebugRenderer::GenDrawThickLine(bool ndt, const maths::Vector3& start, const maths::Vector3& end, float line_width, const maths::Vector4& colour)
	{
		auto list = ndt ? &m_DrawListNDT : &m_DrawList;

		//For Depth Sorting
		maths::Vector3 midPoint = (start + end) * 0.5f;
		float camDist = maths::Vector3::Dot(midPoint - m_CameraPosition, midPoint - m_CameraPosition);

		//Add to Data Structures
		list->_vThickLines.emplace_back(start, line_width);
		list->_vThickLines.push_back(colour);

		list->_vThickLines.emplace_back(end, camDist);
		list->_vThickLines.push_back(colour);

		GenDrawPoint(ndt, start, line_width * 0.5f, colour);
		GenDrawPoint(ndt, end, line_width * 0.5f, colour);
	}
	void DebugRenderer::DrawThickLine(const maths::Vector3& start, const maths::Vector3& end, float line_width, const maths::Vector3& colour)
	{
		GenDrawThickLine(false, start, end, line_width, maths::Vector4(colour, 1.0f));
	}
	void DebugRenderer::DrawThickLine(const maths::Vector3& start, const maths::Vector3& end, float line_width, const maths::Vector4& colour)
	{
		GenDrawThickLine(false, start, end, line_width, colour);
	}
	void DebugRenderer::DrawThickLineNDT(const maths::Vector3& start, const maths::Vector3& end, float line_width, const maths::Vector3& colour)
	{
		GenDrawThickLine(true, start, end, line_width, maths::Vector4(colour, 1.0f));
	}
	void DebugRenderer::DrawThickLineNDT(const maths::Vector3& start, const maths::Vector3& end, float line_width, const maths::Vector4& colour)
	{
		GenDrawThickLine(true, start, end, line_width, colour);
	}

	//Draw line with thickness of 1 screen pixel regardless of distance from camera
	void DebugRenderer::GenDrawHairLine(bool ndt, const maths::Vector3& start, const maths::Vector3& end, const maths::Vector4& colour)
	{
		auto list = ndt ? &m_DrawListNDT : &m_DrawList;
		list->_vHairLines.emplace_back(start, 1.0f);
		list->_vHairLines.push_back(colour);

		list->_vHairLines.emplace_back(end, 1.0f);
		list->_vHairLines.push_back(colour);
	}
	void DebugRenderer::DrawHairLine(const maths::Vector3& start, const maths::Vector3& end, const maths::Vector3& colour)
	{
		GenDrawHairLine(false, start, end, maths::Vector4(colour, 1.0f));
	}
	void DebugRenderer::DrawHairLine(const maths::Vector3& start, const maths::Vector3& end, const maths::Vector4& colour)
	{
		GenDrawHairLine(false, start, end, colour);
	}
	void DebugRenderer::DrawHairLineNDT(const maths::Vector3& start, const maths::Vector3& end, const maths::Vector3& colour)
	{
		GenDrawHairLine(true, start, end, maths::Vector4(colour, 1.0f));
	}
	void DebugRenderer::DrawHairLineNDT(const maths::Vector3& start, const maths::Vector3& end, const maths::Vector4& colour)
	{
		GenDrawHairLine(true, start, end, colour);
	}

	//Draw Matrix (x,y,z axis at pos)
	void DebugRenderer::DrawMatrix(const maths::Matrix4& mtx)
	{
		maths::Vector3 position = mtx.GetPositionVector();
		GenDrawHairLine(false, position, position + maths::Vector3(mtx.values[0], mtx.values[1], mtx.values[2]), maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		GenDrawHairLine(false, position, position + maths::Vector3(mtx.values[4], mtx.values[5], mtx.values[6]), maths::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		GenDrawHairLine(false, position, position + maths::Vector3(mtx.values[8], mtx.values[9], mtx.values[10]), maths::Vector4(0.0f, 0.0f, 1.0f, 1.0f));
	}
	void DebugRenderer::DrawMatrix(const maths::Matrix3& mtx, const maths::Vector3& position)
	{
		GenDrawHairLine(false, position, position + mtx.GetCol(0), maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		GenDrawHairLine(false, position, position + mtx.GetCol(1), maths::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		GenDrawHairLine(false, position, position + mtx.GetCol(2), maths::Vector4(0.0f, 0.0f, 1.0f, 1.0f));
	}
	void DebugRenderer::DrawMatrixNDT(const maths::Matrix4& mtx)
	{
		maths::Vector3 position = mtx.GetPositionVector();
		GenDrawHairLine(true, position, position + maths::Vector3(mtx.values[0], mtx.values[1], mtx.values[2]), maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		GenDrawHairLine(true, position, position + maths::Vector3(mtx.values[4], mtx.values[5], mtx.values[6]), maths::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		GenDrawHairLine(true, position, position + maths::Vector3(mtx.values[8], mtx.values[9], mtx.values[10]), maths::Vector4(0.0f, 0.0f, 1.0f, 1.0f));
	}
	void DebugRenderer::DrawMatrixNDT(const maths::Matrix3& mtx, const maths::Vector3& position)
	{
		GenDrawHairLine(true, position, position + mtx.GetCol(0), maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		GenDrawHairLine(true, position, position + mtx.GetCol(1), maths::Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		GenDrawHairLine(true, position, position + mtx.GetCol(2), maths::Vector4(0.0f, 0.0f, 1.0f, 1.0f));
	}

	//Draw Triangle
	void DebugRenderer::GenDrawTriangle(bool ndt, const maths::Vector3& v0, const maths::Vector3& v1, const maths::Vector3& v2, const maths::Vector4& colour)
	{
		auto list = ndt ? &m_DrawListNDT : &m_DrawList;

		//For Depth Sorting
		maths::Vector3 midPoint = (v0 + v1 + v2) * (1.0f / 3.0f);
		float camDist = maths::Vector3::Dot(midPoint - m_CameraPosition, midPoint - m_CameraPosition);

		//Add to data structures
		list->_vTris.emplace_back(v0, camDist);
		list->_vTris.push_back(colour);

		list->_vTris.emplace_back(v1, 1.0f);
		list->_vTris.push_back(colour);

		list->_vTris.emplace_back(v2, 1.0f);
		list->_vTris.push_back(colour);
	}

	void DebugRenderer::DrawTriangle(const maths::Vector3& v0, const maths::Vector3& v1, const maths::Vector3& v2, const maths::Vector4& colour)
	{
		GenDrawTriangle(false, v0, v1, v2, colour);
	}

	void DebugRenderer::DrawTriangleNDT(const maths::Vector3& v0, const maths::Vector3& v1, const maths::Vector3& v2, const maths::Vector4& colour)
	{
		GenDrawTriangle(true, v0, v1, v2, colour);
	}

	//Draw Polygon (Renders as a triangle fan, so verts must be arranged in order)
	void DebugRenderer::DrawPolygon(int n_verts, const maths::Vector3* verts, const maths::Vector4& colour)
	{
		for (int i = 2; i < n_verts; ++i)
		{
			GenDrawTriangle(false, verts[0], verts[i - 1], verts[i], colour);
		}
	}

	void DebugRenderer::DrawPolygonNDT(int n_verts, const maths::Vector3* verts, const maths::Vector4& colour)
	{
		for (int i = 2; i < n_verts; ++i)
		{
			GenDrawTriangle(true, verts[0], verts[i - 1], verts[i], colour);
		}
	}

	void DebugRenderer::DrawTextCs(const maths::Vector4& cs_pos, const float font_size, const std::string& text, const TextAlignment alignment, const maths::Vector4& colour)
	{
		maths::Vector3 cs_size = maths::Vector3(font_size / m_Width, font_size / m_Height, 0.0f);
		cs_size = cs_size * cs_pos.GetW();

		//Work out the starting position of text based off desired alignment
		float x_offset = 0.0f;
		const auto text_len = static_cast<int>(text.length());

		switch (alignment)
		{
		case TEXTALIGN_RIGHT:
			x_offset = -text_len * cs_size.GetX() * 1.2f;
			break;

		case TEXTALIGN_CENTRE:
			x_offset = -text_len * cs_size.GetX() * 0.6f;
			break;
		default:;

			//   case TEXTALIGN_LEFT:
			//     x_offset = -text_len * cs_size.GetX() * 0.6f;
		}

		//Add each characters to the draw list individually
		for (int i = 0; i < text_len; ++i)
		{
			maths::Vector4 char_pos = maths::Vector4(cs_pos.GetX() + x_offset, cs_pos.GetY(), cs_pos.GetZ(), cs_pos.GetW());
			maths::Vector4 char_data = maths::Vector4(cs_size.GetX(), cs_size.GetY(), static_cast<float>(text[i]), 0.0f);

			m_vChars.push_back(char_pos);
			m_vChars.push_back(char_data);
			m_vChars.push_back(colour);
			m_vChars.push_back(colour);	//We dont really need this, but we need the padding to match the same vertex format as all the other debug drawables

			x_offset += cs_size.GetX() * 1.2f;
		}
	}

	//Draw Text WorldSpace
	void DebugRenderer::DrawTextWs(const maths::Vector3& pos, const float font_size, const TextAlignment alignment, const maths::Vector4& colour, const std::string text, ...)
	{
		va_list args;
		va_start(args, text);

		char buf[1024];

		int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

		va_end(args);

		int length = (needed < 0) ? 1024 : needed;

		std::string formatted_text = std::string(buf, static_cast<size_t>(length));

		maths::Vector4 cs_pos = m_ProjViewMtx * maths::Vector4(pos, 1.0f);
		DrawTextCs(cs_pos, font_size, formatted_text, alignment, colour);
	}

	void DebugRenderer::DrawTextWsNDT(const maths::Vector3& pos, const float font_size, const TextAlignment alignment, const maths::Vector4& colour, const std::string text, ...)
	{
		va_list args;
		va_start(args, text);

		char buf[1024];

		int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

		va_end(args);

		int length = (needed < 0) ? 1024 : needed;

		String formatted_text = String(buf, static_cast<size_t>(length));

		maths::Vector4 cs_pos = m_ProjViewMtx * maths::Vector4(pos, 1.0f);
		cs_pos.SetZ(1.0f * cs_pos.GetW());
		DrawTextCs(cs_pos, font_size, formatted_text, alignment, colour);
	}

	//Status Entry
	void DebugRenderer::AddStatusEntry(const maths::Vector4& colour, const std::string text, ...)
	{
		float cs_size_x = STATUS_TEXT_SIZE / m_Width * 2.0f;
		float cs_size_y = STATUS_TEXT_SIZE / m_Height * 2.0f;

		va_list args;
		va_start(args, text);

		char buf[1024];

		int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

		va_end(args);

		int length = (needed < 0) ? 1024 : needed;

		std::string formatted_text = std::string(buf, static_cast<size_t>(length));

		DrawTextCs(maths::Vector4(-1.0f + cs_size_x * 0.5f, 1.0f - (m_NumStatusEntries * cs_size_y) - cs_size_y, -1.0f, 1.0f), STATUS_TEXT_SIZE, formatted_text, TEXTALIGN_LEFT, colour);
		m_NumStatusEntries++;
		m_MaxStatusEntryWidth = maths::Max(m_MaxStatusEntryWidth, cs_size_x * 0.6f * length);
	}

	//Log

	void DebugRenderer::AddLogEntry(const maths::Vector3& colour, const std::string& text)
	{
		/*	time_t now = time(0);
		tm ltm;
		localtime_s(&ltm, &now);*/

		std::stringstream ss;
		//ss << "[" << ltm.tm_hour << ":" << ltm.tm_min << ":" << ltm.tm_sec << "] ";

		LogEntry le;
		le.text = ss.str() + text;
		le.colour = maths::Vector4(colour.GetX(), colour.GetY(), colour.GetZ(), 1.0f);

		if (m_vLogEntries.size() < MAX_LOG_SIZE)
			m_vLogEntries.push_back(le);
		else
		{
			m_vLogEntries[m_LogEntriesOffset] = le;
			m_LogEntriesOffset = (m_LogEntriesOffset + 1) % MAX_LOG_SIZE;
		}

		JM_CORE_WARN(text);
	}

	void DebugRenderer::Log(const maths::Vector3& colour, const std::string text, ...)
	{
		va_list args;
		va_start(args, text);

		char buf[1024];

		int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

		va_end(args);

		int length = (needed < 0) ? 1024 : needed;
		AddLogEntry(colour, std::string(buf, static_cast<size_t>(length)));
	}

	void DebugRenderer::Log(const std::string text, ...)
	{
		va_list args;
		va_start(args, text);

		char buf[1024];

		int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

		va_end(args);

		int length = (needed < 0) ? 1024 : needed;
		AddLogEntry(maths::Vector3(0.4f, 1.0f, 0.6f), std::string(buf, static_cast<size_t>(length)));
	}

	void DebugRenderer::LogE(const char* filename, int linenumber, const std::string text, ...)
	{
		//Error Format:
		//<text>
		//		-> <line number> : <file name>

		va_list args;
		va_start(args, text);

		char buf[1024];

		int needed = VSNPRINTF(buf, 1023, _TRUNCATE, text.c_str(), args);

		va_end(args);

		int length = (needed < 0) ? 1024 : needed;

		Log(maths::Vector3(1.0f, 0.25f, 0.25f), "[ERROR] %s:%d", filename, linenumber);
		AddLogEntry(maths::Vector3(1.0f, 0.5f, 0.5f), "\t \x01 \"" + std::string(buf, static_cast<size_t>(length)) + "\"");

		std::cout << std::endl;
	}

	void DebugRenderer::DebugDraw(maths::BoundingBox* box, const maths::Vector4 &edgeColour, float width)
	{
		maths::Vector3 uuu = box->Upper();
		maths::Vector3 lll = box->Lower();

		maths::Vector3 ull(uuu.GetX(), lll.GetY(), lll.GetZ());
		maths::Vector3 uul(uuu.GetX(), uuu.GetY(), lll.GetZ());
		maths::Vector3 ulu(uuu.GetX(), lll.GetY(), uuu.GetZ());

		maths::Vector3 luu(lll.GetX(), uuu.GetY(), uuu.GetZ());
		maths::Vector3 llu(lll.GetX(), lll.GetY(), uuu.GetZ());
		maths::Vector3 lul(lll.GetX(), uuu.GetY(), lll.GetZ());

		// Draw edges
		DrawThickLineNDT(luu, uuu, width, edgeColour);
		DrawThickLineNDT(lul, uul, width, edgeColour);
		DrawThickLineNDT(llu, ulu, width, edgeColour);
		DrawThickLineNDT(lll, ull, width, edgeColour);

		DrawThickLineNDT(lul, lll, width, edgeColour);
		DrawThickLineNDT(uul, ull, width, edgeColour);
		DrawThickLineNDT(luu, llu, width, edgeColour);
		DrawThickLineNDT(uuu, ulu, width, edgeColour);

		DrawThickLineNDT(lll, llu, width, edgeColour);
		DrawThickLineNDT(ull, ulu, width, edgeColour);
		DrawThickLineNDT(lul, luu, width, edgeColour);
		DrawThickLineNDT(uul, uuu, width, edgeColour);
	}

	void DebugRenderer::DebugDraw(maths::BoundingSphere* sphere, const maths::Vector4 &colour)
	{
		jm::DebugRenderer::DrawPointNDT(sphere->Centre(), sphere->SphereRadius(), colour);
	}

	void DebugRenderer::ClearDebugLists()
	{
		m_vChars.clear();

		const auto clear_list = [](DebugRenderer::DebugDrawList& list)
		{
			list._vPoints.clear();
			list._vThickLines.clear();
			list._vHairLines.clear();
			list._vTris.clear();
		};
		clear_list(m_DrawList);
		clear_list(m_DrawListNDT);

		m_NumStatusEntries = 0;
		m_MaxStatusEntryWidth = 0.0f;
	}

	void DebugRenderer::ClearLog()
	{
		m_vLogEntries.clear();
		m_LogEntriesOffset = 0;
	}

	struct PointVertex
	{
		maths::Vector4 pos;
		maths::Vector4 col;
	};

	struct LineVertex
	{
		PointVertex p0;
		PointVertex p1;
	};

	struct TriVertex
	{
		PointVertex p0;
		PointVertex p1;
		PointVertex p2;
	};

	void DebugRenderer::SortDebugLists()
	{
		//Draw log text
		float cs_size_x = LOG_TEXT_SIZE / m_Width * 2.0f;
		float cs_size_y = LOG_TEXT_SIZE / m_Height * 2.0f;
		size_t log_len = m_vLogEntries.size();

		float max_x = 0.0f;
		for (size_t i = 0; i < log_len; ++i)
		{
			max_x = maths::Max(max_x, m_vLogEntries[i].text.length() * cs_size_x * 0.6f);

			size_t idx = (i + m_LogEntriesOffset) % MAX_LOG_SIZE;

			DrawTextCs(maths::Vector4(-1.0f + cs_size_x * 0.5f, -1.0f + ((log_len - i - 1) * cs_size_y) + cs_size_y, 0.0f, 1.0f), LOG_TEXT_SIZE, m_vLogEntries[idx].text, TEXTALIGN_LEFT, m_vLogEntries[idx].colour);
		}

		auto sort_lists = [](DebugRenderer::DebugDrawList& list)
		{
			//Sort Points
			if (!list._vPoints.empty())
			{
				auto * points = reinterpret_cast<PointVertex*>(&list._vPoints[0]);
				std::sort(points, points + list._vPoints.size() / 2, [&](const PointVertex& a, const PointVertex& b)
				{
					float a2 = maths::Vector3::Dot(a.pos.ToVector3() - m_CameraPosition, a.pos.ToVector3() - m_CameraPosition);
					float b2 = maths::Vector3::Dot(b.pos.ToVector3() - m_CameraPosition, b.pos.ToVector3() - m_CameraPosition);
					return (a2 > b2);
				});
			}

			//Sort Lines
			if (!list._vThickLines.empty())
			{
				auto * lines = reinterpret_cast<LineVertex*>(&list._vThickLines[0]);
				std::sort(lines, lines + list._vThickLines.size() / 4, [](const LineVertex& a, const LineVertex& b)
				{
					return (a.p1.pos.GetW() > b.p1.pos.GetW());
				});
			}

			//Sort Triangles
			if (!list._vTris.empty())
			{
				auto * tris = reinterpret_cast<TriVertex*>(&list._vTris[0]);
				std::sort(tris, tris + list._vTris.size() / 6, [](const TriVertex& a, const TriVertex& b)
				{
					return (a.p0.pos.GetW() > b.p0.pos.GetW());
				});
			}
			return false;
		};

		sort_lists(m_DrawList);
		sort_lists(m_DrawListNDT);

		//Draw background to text areas
		// - This is done last as to avoid additional triangle-sorting
		maths::Matrix4 invProjView = maths::Matrix4::Inverse(m_ProjViewMtx);
		float rounded_offset_x = 10.f / m_Width * 2.0f;
		float rounded_offset_y = 10.f / m_Height * 2.0f;
		const maths::Vector4 log_background_col(0.1f, 0.1f, 0.1f, 0.5f);

		maths::Vector3 centre;
		maths::Vector3 last;
		const auto NextTri = [&](const maths::Vector3& point)
		{
			DrawTriangleNDT(centre, last, point, log_background_col);
			DrawHairLineNDT(last, point);
			last = point;
		};

		//Draw Log Background
		if (!m_vLogEntries.empty())
		{
			float top_y = -1 + m_vLogEntries.size() * cs_size_y + cs_size_y;
			max_x = max_x - 1 + cs_size_x;

			centre = invProjView * maths::Vector3(-1, -1, 0);
			last = invProjView * maths::Vector3(max_x, -1, 0);
			NextTri(invProjView * maths::Vector3(max_x, top_y - rounded_offset_y, 0.0f));
			for (int i = 0; i < 5; ++i)
			{
				maths::Vector3 round_offset = maths::Vector3(
						cos(maths::DegToRad(i * 22.5f)) * rounded_offset_x,
						sin(maths::DegToRad(i * 22.5f)) * rounded_offset_y,
					0.0f);
				NextTri(invProjView * maths::Vector3(max_x + round_offset.GetX() - rounded_offset_x, top_y + round_offset.GetY() - rounded_offset_y, 0.0f));
			}
			NextTri(invProjView * maths::Vector3(-1, top_y, 0.0f));
		}

		//Draw Status Background
		if (m_NumStatusEntries > 0)
		{
			cs_size_x = STATUS_TEXT_SIZE / m_Width * 2.0f;
			cs_size_y = STATUS_TEXT_SIZE / m_Height * 2.0f;

			const float btm_y = 1 - m_NumStatusEntries * cs_size_y - cs_size_y;
			max_x = -1 + cs_size_x + m_MaxStatusEntryWidth;

			centre = invProjView * maths::Vector3(-1, 1, 0);
			last = invProjView * maths::Vector3(-1, btm_y, 0);

			NextTri(invProjView * maths::Vector3(max_x - rounded_offset_x, btm_y, 0.0f));
			for (int i = 4; i >= 0; --i)
			{
				const maths::Vector3 round_offset = maths::Vector3(
					cos(maths::DegToRad(i * 22.5f)) * rounded_offset_x,
					sin(maths::DegToRad(i * 22.5f)) * rounded_offset_y,
					0.0f);
				NextTri(invProjView * maths::Vector3(max_x + round_offset.GetX() - rounded_offset_x, btm_y - round_offset.GetY() + rounded_offset_y, 0.0f));
			}
			NextTri(invProjView * maths::Vector3(max_x, 1, 0.0f));
		}
	}

	void DebugRenderer::DrawDebugLists()
	{
		Renderer::SetBlend(true);
		Renderer::SetBlendFunction(RendererBlendFunction::SOURCE_ALPHA, RendererBlendFunction::ONE_MINUS_SOURCE_ALPHA);
		//	Renderer::SetCulling(false);

		//Buffer all data into the single buffer object
		size_t max_size = 0;
		max_size += m_DrawList._vPoints.size() + m_DrawList._vThickLines.size() + m_DrawList._vHairLines.size() + m_DrawList._vTris.size();
		max_size += m_DrawListNDT._vPoints.size() + m_DrawListNDT._vThickLines.size() + m_DrawListNDT._vHairLines.size() + m_DrawListNDT._vTris.size();
		max_size += m_vChars.size();
		max_size *= sizeof(maths::Vector4);

		size_t buffer_offsets[8];
		//DT Draw List
		buffer_offsets[0] = 0;
		buffer_offsets[1] = m_DrawList._vPoints.size();
		buffer_offsets[2] = buffer_offsets[1] + m_DrawList._vThickLines.size();
		buffer_offsets[3] = buffer_offsets[2] + m_DrawList._vHairLines.size();

		//NDT Draw List
		buffer_offsets[4] = buffer_offsets[3] + m_DrawList._vTris.size();
		buffer_offsets[5] = buffer_offsets[4] + m_DrawListNDT._vPoints.size();
		buffer_offsets[6] = buffer_offsets[5] + m_DrawListNDT._vThickLines.size();
		buffer_offsets[7] = buffer_offsets[6] + m_DrawListNDT._vHairLines.size();

		//Char Offset
		m_OffsetChars = buffer_offsets[7] + m_DrawListNDT._vTris.size();

		const size_t stride = sizeof(maths::Vector4);

		m_VertexArray->Bind();
		m_VertexArray->GetBuffer(0)->SetData(static_cast<uint>(max_size), nullptr);

		const auto buffer_drawlist = [&](DebugRenderer::DebugDrawList& list, size_t* offsets)
		{
			if (!list._vPoints.empty()) m_VertexArray->GetBuffer(0)->SetDataSub(static_cast<uint>(list._vPoints.size() * stride), &list._vPoints[0], static_cast<uint>(offsets[0] * stride));
			if (!list._vThickLines.empty()) m_VertexArray->GetBuffer(0)->SetDataSub(static_cast<uint>(list._vThickLines.size() * stride), &list._vThickLines[0], static_cast<uint>(offsets[1] * stride));
			if (!list._vHairLines.empty()) m_VertexArray->GetBuffer(0)->SetDataSub(static_cast<uint>(list._vHairLines.size() * stride), &list._vHairLines[0], static_cast<uint>(offsets[2] * stride));
			if (!list._vTris.empty()) m_VertexArray->GetBuffer(0)->SetDataSub(static_cast<uint>(list._vTris.size() * stride), &list._vTris[0], static_cast<uint>(offsets[3] * stride));
		};
		buffer_drawlist(m_DrawList, &buffer_offsets[0]);
		buffer_drawlist(m_DrawListNDT, &buffer_offsets[4]);
		if (!m_vChars.empty()) m_VertexArray->GetBuffer(0)->SetDataSub(static_cast<uint>(m_vChars.size() * stride), &m_vChars[0], static_cast<uint>(m_OffsetChars * stride));

		//float aspectRatio = (float)(m_Height / m_Width);

		const auto render_drawlist = [&](DebugRenderer::DebugDrawList& list, size_t* offsets, uint OffsetID)
		{
			if (m_pShaderPoints && !list._vPoints.empty())
			{
			//	m_pShaderPoints->SetUniform("uProjMtx", m_ProjMtx);
			//	m_pShaderPoints->SetUniform("uViewMtx", m_ViewMtx);
			//	m_pShaderPoints->Bind();

				Renderer::DrawArrays(DrawType::POINT, static_cast<uint>(offsets[0 + OffsetID]) >> 1, static_cast<uint>(list._vPoints.size()) >> 1);

			//	m_pShaderPoints->Unbind();
			}

			if (m_pShaderLines && !list._vThickLines.empty())
			{
			//	m_pShaderLines->SetUniform("uProjViewMtx", m_ProjViewMtx);
			//	m_pShaderLines->SetUniform("uAspect", aspectRatio);
			//	m_pShaderLines->Bind();

				Renderer::DrawArrays(DrawType::LINES, static_cast<int>(offsets[1 + OffsetID]) >> 1, static_cast<int>(list._vThickLines.size()) >> 1);

			//	m_pShaderLines->Unbind();
			}

			if (m_pShaderHairLines && (list._vHairLines.size() + list._vTris.size()) > 0)
			{
			//	m_pShaderHairLines->SetUniform("uProjViewMtx", m_ProjViewMtx);
			//	m_pShaderHairLines->Bind();

				if (!list._vHairLines.empty()) Renderer::DrawArrays(DrawType::LINES, static_cast<uint>(offsets[2 + OffsetID]) >> 1, static_cast<uint>(list._vHairLines.size()) >> 1);
				if (!list._vTris.empty()) Renderer::DrawArrays(DrawType::TRIANGLE, static_cast<uint>(offsets[3 + OffsetID]) >> 1, static_cast<uint>(list._vTris.size()) >> 1);

			//	m_pShaderHairLines->Unbind();
			}
		};

		m_VertexArray->Bind();

		render_drawlist(m_DrawList, buffer_offsets, 0);

		Renderer::SetDepthTesting(false);
		render_drawlist(m_DrawListNDT, buffer_offsets, 4);
		Renderer::SetDepthTesting(true);

		m_VertexArray->Unbind();
	}

	//This Function has been abstracted from previous set of draw calls as to avoid supersampling the font bitmap.. which is bad enough as it is.
	void DebugRenderer::DrawDebubHUD()
	{
		//All text data already updated in main DebugDrawLists
		// - we just need to rebind and draw it

		if (m_pShaderText && !m_vChars.empty())
		{
			m_VertexArray->Bind();
			//m_pShaderText->SetTexture("uFontTex", m_FontTex);
			//m_pShaderText->Bind();

			Renderer::DrawArrays(DrawType::LINES, static_cast<uint>(m_OffsetChars) >> 1, static_cast<uint>(m_vChars.size()) >> 1);

			m_VertexArray->Unbind();
			//m_pShaderText->Unbind();
		}
	}

	void DebugRenderer::Init()
	{
		m_pShaderLines = new Material();//std::shared_ptr<Shader>(Shader::CreateFromFile("DebugLine", "/Shaders/DebugDraw/DebugLine")));
		m_pShaderPoints = new Material();//std::shared_ptr<Shader>(Shader::CreateFromFile("DebugPoint", "/Shaders/DebugDraw/DebugPoint")));
		m_pShaderHairLines = new Material();//std::shared_ptr<Shader>(Shader::CreateFromFile("DebugHairLine", "/Shaders/DebugDraw/DebugHairLine")));
		m_pShaderText = new Material();//std::shared_ptr<Shader>(Shader::CreateFromFile("DebugText", "/Shaders/DebugDraw/DebugText")));

		m_VertexArray = VertexArray::Create();
		m_VertexArray->Bind();

		VertexBuffer* buffer = VertexBuffer::Create(BufferUsage::STATIC);
		buffer->SetData(0, nullptr);

		graphics::BufferLayout layout;
		layout.Push<maths::Vector4>("position");
		layout.Push<maths::Vector4>("colour");
		buffer->SetLayout(layout);

		m_VertexArray->PushBuffer(buffer);

		//Load Font Texture
		m_FontTex = Texture2D::CreateFromFile("Debug Font", "/Textures/font512.png", TextureParameters(), TextureLoadOptions());
		if (!m_FontTex)
		{
			JM_CORE_ERROR("JMDebug could not load font texture", "");
		}
	}

	void DebugRenderer::Release()
	{
		if (m_pShaderPoints)
		{
			delete m_pShaderPoints;
			m_pShaderPoints = nullptr;
		}

		if (m_pShaderLines)
		{
			delete m_pShaderLines;
			m_pShaderLines = nullptr;
		}

		if (m_pShaderHairLines)
		{
			delete m_pShaderHairLines;
			m_pShaderHairLines = nullptr;
		}

		if (m_pShaderText)
		{
			delete m_pShaderText;
			m_pShaderText = nullptr;
		}

		if (m_FontTex)
		{
			delete m_FontTex;
			m_FontTex = nullptr;
		}

		if (m_VertexArray)
		{
			delete m_VertexArray;
			m_VertexArray = nullptr;
		}
	}
}
