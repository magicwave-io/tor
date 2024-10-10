#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "microrestd.h"

using namespace std;
using namespace ufal::microrestd;


	class route_generator : public response_generator
	{
		public:
	    route_generator() {}

	    virtual bool generate() override
		{
			size_t data_size = data.size();
			data.resize(data_size + 1024);
			f->read(data.data() + data_size, 1024);
			data.resize(data_size + f->gcount());

			// Now sleep for 2 seconds to simulate hard work :-)
			this_thread::sleep_for(chrono::seconds(2));

			return f->gcount();
    }
		
    virtual string_piece current() const override
	{
		return string_piece(data.data(), data.size());
	}
		
    virtual void consume(size_t length) override
	{
		if (length >= data.size()) data.clear();
		else if (length) data.erase(data.begin(), data.begin() + length);
    }

   private:
    unique_ptr<ifstream> f;
    vector<char> data;
  };
