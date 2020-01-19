#include "oneflow/core/job/id_manager.h"

namespace oneflow {

namespace {

static const int64_t machine_id_shl = 11 + 21 + 21;
static const int64_t thread_id_shl = 21 + 21;
static const int64_t local_work_stream_shl = 21;

EnvProto GetEnvProto() {
  EnvProto ret;
  for (size_t i = 0; i < 10; ++i) {
    auto* machine = ret.add_machine();
    machine->set_id(i);
    machine->set_addr("192.168.1." + std::to_string(i));
  }
  ret.set_ctrl_port(9527);
  return ret;
}

Resource GetResource() {
  Resource ret;
  ret.set_machine_num(10);
  ret.set_gpu_device_num(8);
  ret.set_cpu_device_num(5);
  ret.set_comm_net_worker_num(4);
  return ret;
}

void New() {
  Global<EnvDesc>::New(GetEnvProto());
  Global<ResourceDesc>::New(GetResource());
  Global<IDMgr>::New();
}

void Delete() {
  Global<IDMgr>::Delete();
  Global<ResourceDesc>::Delete();
  Global<EnvDesc>::Delete();
}

}  // namespace

TEST(IDMgr, compile_task_id) {
  New();
  int64_t machine1thrd2 =
      (static_cast<int64_t>(1) << machine_id_shl) + (static_cast<int64_t>(2) << thread_id_shl);
  int64_t machine3thrd4 =
      (static_cast<int64_t>(3) << machine_id_shl) + (static_cast<int64_t>(4) << thread_id_shl);
  int64_t local_work_stream1 = (static_cast<int64_t>(1) << local_work_stream_shl);
  int64_t local_work_stream3 = (static_cast<int64_t>(3) << local_work_stream_shl);
  ASSERT_EQ(Global<IDMgr>::Get()->NewTaskId(1, 2, 0), machine1thrd2 | 0 | 0);
  ASSERT_EQ(Global<IDMgr>::Get()->NewTaskId(1, 2, 1), machine1thrd2 | local_work_stream1 | 1);
  ASSERT_EQ(Global<IDMgr>::Get()->NewTaskId(1, 2, 1), machine1thrd2 | local_work_stream1 | 2);
  ASSERT_EQ(Global<IDMgr>::Get()->NewTaskId(3, 4, 1), machine3thrd4 | local_work_stream1 | 0);
  ASSERT_EQ(Global<IDMgr>::Get()->NewTaskId(3, 4, 1), machine3thrd4 | local_work_stream1 | 1);
  ASSERT_EQ(Global<IDMgr>::Get()->NewTaskId(3, 4, 3), machine3thrd4 | local_work_stream3 | 2);
  Delete();
}

TEST(IDMgr, compile_regst_desc_id) {
  New();
  ASSERT_EQ(Global<IDMgr>::Get()->NewRegstDescId(), 0);
  ASSERT_EQ(Global<IDMgr>::Get()->NewRegstDescId(), 1);
  ASSERT_EQ(Global<IDMgr>::Get()->NewRegstDescId(), 2);
  Delete();
}

TEST(IDMgr, runtime_machine_id) {
  New();
  int64_t actor_id5_machine1thrd3 =
      (static_cast<int64_t>(1) << machine_id_shl)           // machine_id_1
      + (static_cast<int64_t>(3) << thread_id_shl)          // thrd_id_3
      + (static_cast<int64_t>(1) << local_work_stream_shl)  // work_stream_id_1
      + 5;                                                  // actor_id_5
  ASSERT_EQ(Global<IDMgr>::Get()->MachineId4ActorId(actor_id5_machine1thrd3), 1);
  Delete();
}

TEST(IDMgr, runtime_thrd_id) {
  New();
  int64_t actor_id5_machine1thrd3 = (static_cast<int64_t>(1) << machine_id_shl)   // machine_id_1
                                    + (static_cast<int64_t>(3) << thread_id_shl)  // thrd_id_3
                                    // work_stream_id_0
                                    + 5;  // actor_id_5
  ASSERT_EQ(Global<IDMgr>::Get()->ThrdId4ActorId(actor_id5_machine1thrd3), 3);
  int64_t actor_id6_machine2thrd4 =
      (static_cast<int64_t>(2) << machine_id_shl)             // machine_id_2
      + (static_cast<int64_t>(4) << thread_id_shl)            // thrd_id_4
      + (static_cast<int64_t>(101) << local_work_stream_shl)  // work_stream_id_101
      + 6;                                                    // actor_id_6
  ASSERT_EQ(Global<IDMgr>::Get()->ThrdId4ActorId(actor_id6_machine2thrd4), 4);
  Delete();
}

}  // namespace oneflow
