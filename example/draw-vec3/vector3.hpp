#pragma once

#include <glm/glm.hpp>
#include <comm/dbg.hpp>

class DrawVec3
{
private:
	glm::vec3 pos = glm::vec3(0.0f,0.0f,0.0f);
	glm::vec3 vec;
	glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f);
	bool needUpdate = false;
	float arrow_rate = 0.2f;
public:
	struct Vertex{
		glm::vec3 pos;
		glm::vec3 color;
	};

	DrawVec3(glm::vec3 vec_) : vec(vec_)
	{
		needUpdate = true;
	}

	void set_pos(glm::vec3& v)
	{
		needUpdate = true;
		this->pos = v;
	}

	glm::vec3 get_pos()
	{
		return this->pos;
	}

	void set_vec(glm::vec3& v)
	{
		needUpdate = true;
		this->vec = v;
	}

	glm::vec3 get_vec()
	{
		return this->vec;
	}

	void set_color(glm::vec3& v)
	{
		needUpdate = true;
		this->color = v;
	}

	glm::vec3 get_color()
	{
		return this->color;
	}

	void set_arrow_rate(float v)
	{
		needUpdate = true;
		this->arrow_rate = v;
	}

	float get_arrow_rate()
	{
		return this->arrow_rate;
	}

	float dot(glm::vec2 v1, glm::vec2 v2)
	{
		return v1.x * v2.x + v1.y * v2.y;
	}

	std::vector<DrawVec3::Vertex> build_vertices()
	{
		std::vector<DrawVec3::Vertex> res;
		needUpdate = false;

		res.push_back({ this->pos,this->color });
	
		res.push_back({ this->pos + this->vec,this->color });
		
		glm::vec3 forword = this->vec;

		glm::vec3 temp_hl = glm::cross(this->vec , glm::vec3(0.0f, -1.0f, 0.0f));
		if ( glm::zero<glm::vec3>() == temp_hl)
		{
			temp_hl = glm::cross(this->vec, glm::vec3(0.0f, 0.0f, -1.0f));
		}
		res.push_back({  res[1].pos + (( glm::normalize( temp_hl ) - forword) * arrow_rate)   ,this->color });

		glm::vec3 temp_hr = glm::cross( glm::vec3(0.0f, -1.0f, 0.0f), this->vec);
		if (glm::zero<glm::vec3>() == temp_hr)
		{
			temp_hr = glm::cross(glm::vec3(0.0f, 0.0f, -1.0f), this->vec);
		}
		res.push_back({ res[1].pos + (( glm::normalize( temp_hr ) - forword) * arrow_rate)   ,this->color });

		glm::vec3 temp_vl = glm::normalize( glm::cross(this->vec, -temp_hl) );
		
		res.push_back({ res[1].pos + ((temp_vl - forword) * arrow_rate)   ,this->color });

		glm::vec3 temp_vr = glm::normalize( glm::cross(-temp_hl, this->vec) );
		
		res.push_back({ res[1].pos + ((temp_vr - forword) * arrow_rate)   ,this->color });
		

		return res;
	}

	std::vector<uint32_t> build_indices()
	{
		return { 0,1,1,2,1,3,1,4,1,5 };
	}

	uint32_t indices_count() { return 10; }
	uint32_t vertices_count() { return 6; }

};