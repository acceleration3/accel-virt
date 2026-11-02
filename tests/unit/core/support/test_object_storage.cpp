#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"

using av::ObjectStorage;
using av::Status;

namespace {

class Counted {
public:
    Counted(int &lives, int value) : lives_(lives), value_(value) { lives_++; }
    ~Counted() { lives_--; }
    Counted(const Counted &) = delete;
    Counted &operator=(const Counted &) = delete;
    int value() const { return value_; }

private:
    int &lives_;
    int  value_;
};

}

TEST(object_storage, starts_empty)
{ const ObjectStorage<Counted> storage;
  CHECK(!storage.constructed()); }

TEST(object_storage, constructs_and_destroys_explicitly)
{ int lives = 0;
  ObjectStorage<Counted> storage;
  CHECK_STATUS(storage.construct(lives, 7), Status::ok);
  CHECK(storage.constructed());
  CHECK_EQ(lives, 1);
  CHECK_EQ(storage.object().value(), 7);
  const ObjectStorage<Counted> &view = storage;
  CHECK_EQ(view.object().value(), 7);
  storage.destroy();
  CHECK(!storage.constructed());
  CHECK_EQ(lives, 0); }

TEST(object_storage, a_second_construct_is_refused_and_destroy_when_empty_does_nothing)
{ int lives = 0;
  ObjectStorage<Counted> storage;
  storage.destroy();
  CHECK_EQ(lives, 0);
  CHECK_STATUS(storage.construct(lives, 1), Status::ok);
  CHECK_STATUS(storage.construct(lives, 2), Status::invalid_state);
  CHECK_EQ(storage.object().value(), 1);
  CHECK_EQ(lives, 1);
  storage.destroy(); }

TEST(object_storage, is_aligned_for_its_object)
{ ObjectStorage<long double> storage;
  CHECK((reinterpret_cast<unsigned long>(storage.address()) % alignof(long double)) == 0u); }

TEST(object_storage_destroy, is_not_constructed_while_the_destructor_runs)
{ struct Asker {
      ObjectStorage<Asker> *storage;
      bool                 *seen;
      ~Asker() { *seen = storage->constructed(); }
  };
  ObjectStorage<Asker> storage;
  bool constructed_during_destruction = true;
  CHECK_STATUS(storage.construct(Asker{&storage, &constructed_during_destruction}), Status::ok);
  storage.destroy();
  CHECK(!constructed_during_destruction);
  CHECK(!storage.constructed()); }
