#include <ipc/endpoint.h>

#include "helpers_com.h"
#include "helpers_sockets.h"
#include "mocks.h"

#include <algorithm>
#include <common/string.h>
#include <common/time.h>
#include <test-helpers/com.h>
#include <test-helpers/helpers.h>
#include <ut/assert.h>
#include <ut/test.h>

using namespace std;
using namespace micro_profiler::tests;

namespace micro_profiler
{
	namespace ipc
	{
		namespace tests
		{
			using micro_profiler::tests::generate_id;

			begin_test_suite( EndpointSelectorTests )
				shared_ptr<mocks::server> session_factory;

				init( Initialize )
				{
					session_factory.reset(new mocks::server);
				}

				init( CheckPortsAreFree )
				{
					assert_is_false(is_local_port_open(6101));
					assert_is_false(is_local_port_open(6102));
					assert_is_false(is_local_port_open(6103));
				}


				test( CreatingSocketServerEndpoint )
				{
					// INIT / ACT
					shared_ptr<void> s1 = run_server("sockets|6102", session_factory);

					// ASSERT
					assert_is_false(is_local_port_open(6101));
					assert_is_true(is_local_port_open(6102));
					assert_is_false(is_local_port_open(6103));

					// INIT / ACT
					shared_ptr<void> s2 = run_server("sockets|6101", session_factory);
					shared_ptr<void> s3 = run_server("sockets|6103", session_factory);

					// ASSERT
					assert_is_true(is_local_port_open(6101));
					assert_is_true(is_local_port_open(6102));
					assert_is_true(is_local_port_open(6103));
				}

#ifdef _WIN32
				test( CreatingCOMServerEndpoint )
				{
					// INIT
					com_initialize ci;
					string ids[] = { to_string(generate_id()), to_string(generate_id()), to_string(generate_id()) };

					// INIT / ACT
					shared_ptr<void> s1 = run_server(("com|" + ids[0]).c_str(), session_factory);

					// ASSERT
					assert_is_true(is_factory_registered(from_string(ids[0])));
					assert_is_false(is_factory_registered(from_string(ids[1])));
					assert_is_false(is_factory_registered(from_string(ids[2])));

					// INIT / ACT
					shared_ptr<void> s2 = run_server(("com|" + ids[1]).c_str(), session_factory);
					shared_ptr<void> s3 = run_server(("com|" + ids[2]).c_str(), session_factory);

					// ASSERT
					assert_is_true(is_factory_registered(from_string(ids[0])));
					assert_is_true(is_factory_registered(from_string(ids[1])));
					assert_is_true(is_factory_registered(from_string(ids[2])));
				}
#else
				test( CreatingCOMServerEndpointFailsOnNonWindows )
				{
					assert_throws(run_server(("com|" + to_string(generate_id())).c_str(), session_factory), protocol_not_supported);
				}
#endif


				test( CreatingWeirdProtocolEndpointFails )
				{
					// ACT / ASSERT
					assert_throws(run_server(("ipx|" + to_string(generate_id())).c_str(), session_factory),
						protocol_not_supported);
					assert_throws(run_server(("dunnowhatthisis|" + to_string(generate_id())).c_str(), session_factory),
						protocol_not_supported);
				}


				test( MalformedEndpointIDIsNotAccepted )
				{
					// ACT / ASSERT
					assert_throws(run_server("", session_factory), invalid_argument);
					assert_throws(run_server("{123-123-123}", session_factory), invalid_argument);
					assert_throws(run_server(0, session_factory), invalid_argument);
				}
			end_test_suite
		}
	}
}