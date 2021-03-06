#include <collector/calls_collector_thread.h>

#include "helpers.h"

#include <test-helpers/helpers.h>
#include <ut/assert.h>
#include <ut/test.h>

using namespace std;
using namespace std::placeholders;

namespace micro_profiler
{
	namespace tests
	{
		namespace
		{
			struct collection_acceptor
			{
				collection_acceptor()
					: total_entries(0)
				{	}

				void accept_calls(const call_record *calls, size_t count)
				{
					collected.push_back(vector<call_record>(calls, calls + count));
					total_entries += count;
				}

				calls_collector_thread::reader_t get_reader()
				{	return bind(&collection_acceptor::accept_calls, this, _1, _2);	}

				size_t total_entries;
				vector< vector<call_record> > collected;
			};

			void emulate_n_calls(calls_collector_thread &collector, size_t calls_number, void *callee)
			{
				virtual_stack vstack;
				timestamp_t timestamp = timestamp_t();

				for (size_t i = 0; i != calls_number; ++i)
				{
					vstack.on_enter(collector, timestamp++, callee);
					vstack.on_exit(collector, timestamp++);
				}
			}
		}


		begin_test_suite( CallsCollectorThreadTests )

			virtual_stack vstack;
			auto_ptr<calls_collector_thread> collector;

			init( ConstructCollector )
			{
				collector.reset(new calls_collector_thread(1000));
			}

			test( CollectNothingOnNoCalls )
			{
				// INIT
				collection_acceptor a;

				// ACT
				collector->read_collected(a.get_reader());

				// ASSERT
				assert_is_empty(a.collected);
			}


			test( CollectEntryExitOnSimpleExternalFunction )
			{
				// INIT
				collection_acceptor a;

				// ACT
				vstack.on_enter(*collector, 100, (void *)0x12345678);
				vstack.on_exit(*collector, 10010);
				collector->read_collected(a.get_reader());

				// ASSERT
				call_record reference[] = {
					{ 100, (void *)0x12345678 }, { 10010, 0 },
				};

				assert_equal(1u, a.collected.size());
				assert_equal(reference, a.collected[0]);
			}


			test( ReadNothingAfterPreviousReadingAndNoCalls )
			{
				// INIT
				collection_acceptor a;

				vstack.on_enter(*collector, 100, (void *)0x12345678);
				vstack.on_exit(*collector, 10010);
				collector->read_collected(a.get_reader());

				// ACT
				collector->read_collected(a.get_reader());
				collector->read_collected(a.get_reader());

				// ASSERT
				assert_equal(1u, a.collected.size());
			}


			test( CollectEntryExitOnNestingFunction )
			{
				// INIT
				collection_acceptor a;

				// ACT
				vstack.on_enter(*collector, 100, (void *)0x12345678);
					vstack.on_enter(*collector, 110, (void *)0xABAB00);
					vstack.on_exit(*collector, 10010);
					vstack.on_enter(*collector, 10011, (void *)0x12345678);
					vstack.on_exit(*collector, 100100);
				vstack.on_exit(*collector, 100105);
				collector->read_collected(a.get_reader());

				// ASSERT
				call_record reference[] = {
					{ 100, (void *)0x12345678 },
						{ 110, (void *)0xABAB00 }, { 10010, 0 },
						{ 10011, (void *)0x12345678 }, { 100100, 0 },
					{ 100105, 0 },
				};

				assert_equal(reference, a.collected[0]);
			}


			test( MaxTraceLengthIsLimited )
			{
				// INIT
				calls_collector_thread c1(67), c2(123), c3(127);
				collection_acceptor a1, a2, a3;

				// ACT (blockage during this test is a failure)
				mt::thread t1(bind(&emulate_n_calls, ref(c1), 670, (void *)0x12345671));
				mt::thread t2(bind(&emulate_n_calls, ref(c2), 1230, (void *)0x12345672));
				mt::thread t3(bind(&emulate_n_calls, ref(c3), 635, (void *)0x12345673));

				while (a1.total_entries < 1340)
				{
					mt::this_thread::sleep_for(mt::milliseconds(30));
					c1.read_collected(a1.get_reader());
				}
				while (a2.total_entries < 2460)
				{
					mt::this_thread::sleep_for(mt::milliseconds(30));
					c2.read_collected(a2.get_reader());
				}
				while (a3.total_entries < 1270)
				{
					mt::this_thread::sleep_for(mt::milliseconds(30));
					c3.read_collected(a3.get_reader());
				}

				// ASSERT
				assert_equal(20u, a1.collected.size());
				assert_equal(20u, a2.collected.size());
				assert_equal(10u, a3.collected.size());

				// INIT
				a1.collected.clear();
				a2.collected.clear();
				a3.collected.clear();

				// ACT
				t1.join();
				t2.join();
				t3.join();
				c1.read_collected(a1.get_reader());
				c2.read_collected(a2.get_reader());
				c3.read_collected(a3.get_reader());

				// ASSERT (no more calls were recorded)
				assert_is_empty(a1.collected);
				assert_is_empty(a2.collected);
				assert_is_empty(a3.collected);
			}


			test( PreviousReturnAddressIsReturnedOnSingleDepthCalls )
			{
				// INIT
				calls_collector_thread c1(1000), c2(1000);
				const void *return_address[] = { (const void *)0x122211, (const void *)0xFF00FF00, };

				// ACT
				on_enter(c1, return_address + 0, 0, 0);
				on_enter(c2, return_address + 1, 0, 0);

				// ACT / ASSERT
				assert_equal(return_address[0], on_exit(c1, return_address + 0, 0));
				assert_equal(return_address[1], on_exit(c2, return_address + 1, 0));
			}


			test( ReturnAddressesAreStoredByValue )
			{
				// INIT
				calls_collector_thread c1(1000), c2(1000);
				const void *return_address[] = { (const void *)0x122211, (const void *)0xFF00FF00, };

				on_enter(c1, return_address + 0, 0, 0);
				on_enter(c2, return_address + 1, 0, 0);

				// ACT
				return_address[0] = 0, return_address[1] = 0;

				// ACT / ASSERT
				assert_equal((const void *)0x122211, on_exit(c1, return_address + 0, 0));
				assert_equal((const void *)0xFF00FF00, on_exit(c2, return_address + 1, 0));
			}


			test( ReturnAddressesAreReturnedInLIFOOrderForNestedCalls )
			{
				// INIT
				const void *return_address[] = {
					(const void *)0x122211, (const void *)0xFF00FF00,
					(const void *)0x222211, (const void *)0x5F00FF00,
					(const void *)0x5F00FF00, (const void *)0x5F00FF00,
				};

				// ACT
				on_enter(*collector, return_address + 5, 0, 0);
				on_enter(*collector, return_address + 3, 0, 0);
				on_enter(*collector, return_address + 2, 0, 0);
				on_enter(*collector, return_address + 1, 0, 0);
				on_enter(*collector, return_address + 0, 0, 0);

				// ACT / ASSERT
				assert_equal(return_address[0], on_exit(*collector, return_address + 0, 0));
				assert_equal(return_address[1], on_exit(*collector, return_address + 1, 0));
				assert_equal(return_address[2], on_exit(*collector, return_address + 2, 0));
				assert_equal(return_address[3], on_exit(*collector, return_address + 3, 0));
				assert_equal(return_address[5], on_exit(*collector, return_address + 5, 0));
			}


			test( ReturnAddressIsPreservedForTailCallOptimization )
			{
				// INIT
				const void *return_address[] = {
					(const void *)0x122211, (const void *)0xFF00FF00,
					(const void *)0x222211, (const void *)0x5F00FF00,
				};

				// ACT
				on_enter(*collector, return_address + 2, 0, 0);
				on_enter(*collector, return_address + 1, 0, 0);
				return_address[1] = (const void *)0x12345;
				on_enter(*collector, return_address + 1, 0, 0);
				on_enter(*collector, return_address + 0, 0, 0);
				return_address[2] = (const void *)0x52345;
				on_enter(*collector, return_address + 0, 0, 0);

				// ACT / ASSERT
				assert_equal((const void *)0x122211, on_exit(*collector, return_address + 0, 0));
				assert_equal((const void *)0xFF00FF00, on_exit(*collector, return_address + 1, 0));
				assert_equal((const void *)0x222211, on_exit(*collector, return_address + 2, 0));
			}

		
			test( FunctionExitIsRecordedOnTailCallOptimization )
			{
				// INIT
				const void *return_address[] = {
					(const void *)0x122211, (const void *)0xFF00FF00,
					(const void *)0x222211, (const void *)0x5F00FF00,
				};
				collection_acceptor a;

				// ACT
				on_enter(*collector, return_address + 0, 0, (void *)1);
				on_enter(*collector, return_address + 1, 11, (void *)2);
				return_address[1] = (const void *)0x12345;
				on_enter(*collector, return_address + 1, 120, (void *)3);

				// ASSERT
				collector->read_collected(a.get_reader());

				call_record reference1[] = {
					{ 0, (void *)1 },
						{ 11, (void *)2 }, { 120, 0 },
						{ 120, (void *)3 },
				};

				assert_equal(reference1, a.collected[0]);

				// ACT
				on_enter(*collector, return_address + 2, 140, (void *)4);
				on_enter(*collector, return_address + 3, 150, (void *)5);
				return_address[3] = (const void *)0x777;
				on_enter(*collector, return_address + 3, 1000, (void *)6);

				// ASSERT
				collector->read_collected(a.get_reader());

				call_record reference2[] = {
							{ 140, (void *)4 },
								{ 150, (void *)5 }, { 1000, 0 },
								{ 1000, (void *)6 },
				};

				assert_equal(reference2, a.collected[1]);
			}


			test( StackGetsUnwoundOnSkippedFunctionExitsAccordinglyToTheStackPointer )
			{
				// INIT
				const void *return_address[] = {
					(const void *)0x122211, (const void *)0xFF00FF00,
					(const void *)0x222211, (const void *)0x5F00FF00,
					(const void *)0x5F9FF11, (const void *)0x0,
				};
				collection_acceptor a;

				on_enter(*collector, return_address + 4, 0, (void *)1);
				on_enter(*collector, return_address + 3, 11, (void *)1);
				on_enter(*collector, return_address + 2, 19191911, (void *)1);
				on_enter(*collector, return_address + 1, 19192911, (void *)1);
				on_enter(*collector, return_address + 0, 19193911, (void *)1);

				// ACT / ASSERT (two entries at once)
				assert_equal((void *)0xFF00FF00, on_exit(*collector, return_address + 1, 19195911));

				// ASSERT
				collector->read_collected(a.get_reader());

				call_record reference1[] = {
					{ 0, (void *)1 },
						{ 11, (void *)1 },
							{ 19191911, (void *)1 },
								{ 19192911, (void *)1 },
									{ 19193911, (void *)1 },
									{ 19195911, 0 },
								{ 19195911, 0 },
				};

				assert_equal(reference1, a.collected[0]);

				// ACT / ASSERT (three entries, we go above the tracked stack - this happen with callee-emptied conventions)
				assert_equal((void *)0x5F9FF11, on_exit(*collector, return_address + 5, 29195911));

				// ASSERT
				collector->read_collected(a.get_reader());

				call_record reference2[] = {
							{ 29195911, 0 },
						{ 29195911, 0 },
					{ 29195911, 0 },
				};

				assert_equal(reference2, a.collected[1]);
			}
		end_test_suite
	}
}
