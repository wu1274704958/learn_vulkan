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

	/*	res.push_back({ this->pos,this->color });
		glm::vec3 head_c = glm::vec3(1.0f);
		res.push_back({ this->pos + this->vec,head_c });*/
		
		glm::vec3 src_bian = -(this->vec * this->arrow_rate);

		glm::mat4 mat(1.0f);

		glm::vec3 vec_nor = glm::normalize(this->vec);

		float rad_z = dbg(glm::acos(  glm::dot(glm::vec2(vec_nor.x, vec_nor.y), glm::vec2(1.0f, 0.0f)) ));
		float rad_y = dbg(glm::acos(  glm::dot(glm::vec2(vec_nor.x, vec_nor.z), glm::vec2(1.0f, 0.0f)) ));

		if (vec.x == 0.0f)
		{
			rad_z = dbg(glm::acos(glm::dot(glm::vec2(vec_nor.z, vec_nor.y), glm::vec2(1.0f, 0.0f))));
			rad_y = dbg(glm::acos(glm::dot(glm::vec2(vec_nor.z, vec_nor.x), glm::vec2(1.0f, 0.0f))));
			
			if (vec_nor.z == 0.0f && vec_nor.x == 0.0f)
				rad_y = 0.0f;
			if (vec_nor.z == 0.0f && vec_nor.y == 0.0f)
				rad_z = 0.0f;

			if (vec_nor.x == 0.0f)
				rad_y = 0.0f;
			if (vec_nor.y == 0.0f)
				rad_z = 0.0f;

		}else
		{
			if (vec_nor.x == 0.0f && vec_nor.z == 0.0f)
				rad_y = 0.0f;
			if (vec_nor.x == 0.0f && vec_nor.y == 0.0f)
				rad_z = 0.0f;
			

			if (vec_nor.z == 0.0f)
				rad_y = 0.0f;
			if (vec_nor.y == 0.0f)
				rad_z = 0.0f;
		}

		dbg(vec_nor.x);
		dbg(vec_nor.y);
		dbg(vec_nor.z);

		
		

		if (vec_nor.y < 0)
			rad_z = glm::pi<float>() + (glm::pi<float>() - rad_z);

		if (vec_nor.z < 0)
			rad_y = glm::pi<float>() + (glm::pi<float>() - rad_y);
	
		dbg(dbg(rad_z) / (glm::pi<float>() * 2.0f) * 360.0f );
		dbg(dbg(rad_y) / (glm::pi<float>() * 2.0f) * 360.0f );

		glm::mat4 view = glm::mat4(1.0f);

		view = glm::rotate(view, rad_z, glm::vec3(0.0f, 0.0f, 1.0f));
		view = glm::rotate(view, rad_y, glm::vec3(0.0f, 1.0f, 0.0f));
		
		glm::vec3 jx(-glm::length(src_bian),0.0f,0.0f);

		res.push_back({ glm::vec3(0.0f,0.0f,0.0f),  color });

		glm::vec4 r = view * glm::vec4(glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
		res.push_back({ glm::vec3(r.x,r.y,r.z) , glm::vec3(1.0f,1.0f,1.0f) });

		////dbg(jx.x);

		//glm::mat4 m1 = glm::rotate(mat, glm::radians(45.0f),glm::vec3(0.0f, 0.0f, 1.0f));
		//glm::vec4 tv1 = view * m1 * glm::vec4(jx, 1.0f);
		//
		//res.push_back({ glm::vec3(tv1.x, tv1.y, tv1.z) + res[1].pos  ,this->color});

		//glm::mat4 m2 = glm::rotate(mat, glm::radians(-45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		//glm::vec4 tv2 = view *  m2 * glm::vec4(jx, 1.0f);
		//
		//res.push_back({glm::vec3(tv2.x, tv2.y, tv2.z) + res[1].pos, this->color});

		//glm::mat4 m3 = glm::rotate(mat, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//glm::vec4 tv3 = view * m3 * glm::vec4(jx, 1.0f);
		//
		//res.push_back({glm::vec3(tv3.x, tv3.y, tv3.z) + res[1].pos,this->color });

		//glm::mat4 m4 = glm::rotate(mat, glm::radians(-45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//glm::vec4 tv4 = view * m4 * glm::vec4(jx, 1.0f);
		//
		//res.push_back({glm::vec3(tv4.x, tv4.y, tv4.z) + res[1].pos,this->color });

		return res;
	}

	std::vector<uint32_t> build_indices()
	{
		return { 0,1/*,1,2,1,3,1,4,1,5*/ };
	}

	uint32_t indices_count() { return 2; }
	

};