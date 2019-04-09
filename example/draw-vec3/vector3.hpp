#pragma once

#include <glm/glm.hpp>

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

	std::vector<DrawVec3::Vertex> build_vertices()
	{
		std::vector<DrawVec3::Vertex> res;
		needUpdate = false;

		res.push_back({ this->pos,this->color });
		res.push_back({ this->pos + this->vec,this->color });
		
		glm::vec3 src_bian = -(this->vec * this->arrow_rate);

		glm::mat4 mat(1.0f);

		glm::mat4 m1 = glm::rotate(mat, glm::radians(45.0f),glm::vec3(0.0f, 0.0f, 1.0f));
		glm::vec4 tv1 = m1 * glm::vec4(src_bian.x, src_bian.y, src_bian.z, 1.0f);
		res.push_back({ glm::vec3(tv1.x, tv1.y, tv1.z) + res[1].pos  ,this->color});

		glm::mat4 m2 = glm::rotate(mat, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		glm::vec4 tv2 = m2 * glm::vec4(src_bian.x, src_bian.y, src_bian.z, 1.0f);
		res.push_back({glm::vec3(tv2.x, tv2.y, tv2.z) + res[1].pos, this->color});

		glm::mat4 m3 = glm::rotate(mat, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec4 tv3 = m3 * glm::vec4(src_bian.x, src_bian.y, src_bian.z, 1.0f);
		res.push_back({glm::vec3(tv3.x, tv3.y, tv3.z) + res[1].pos,this->color });

		glm::mat4 m4 = glm::rotate(mat, glm::radians(-45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::vec4 tv4 = m4 * glm::vec4(src_bian.x, src_bian.y, src_bian.z, 1.0f);
		res.push_back({glm::vec3(tv4.x, tv4.y, tv4.z) + res[1].pos,this->color });

		return res;
	}

	std::vector<uint32_t> build_indices()
	{
		return { 0,1,1,2,1,3,1,4,1,5 };
	}

	uint32_t indices_count() { return 6; }
	

};