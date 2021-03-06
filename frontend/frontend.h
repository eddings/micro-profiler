//	Copyright (c) 2011-2019 by Artem A. Gevorkyan (gevorkyan.org)
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.

#pragma once

#include <common/noncopyable.h>
#include <common/protocol.h>
#include <functional>
#include <ipc/endpoint.h>
#include <common/pod_vector.h>

namespace micro_profiler
{
	class functions_list;
	class symbol_resolver;

	class frontend : public ipc::channel, noncopyable
	{
	public:
		frontend(ipc::channel &outbound);
		~frontend();

		void disconnect_session() throw();

	public:
		std::function<void(const std::string &process_name, const std::shared_ptr<functions_list> &model)> initialized;
		std::function<void()> released;

	private:
		// ipc::channel methods
		virtual void disconnect() throw();
		virtual void message(const_byte_range payload);

		template <typename DataT>
		void send(commands command, const DataT &data);

	private:
		ipc::channel &_outbound;
		std::shared_ptr<symbol_resolver> _resolver;
		std::shared_ptr<functions_list> _model;
		pod_vector<byte> _buffer;
	};
}
