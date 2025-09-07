#include <atomic>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>

#include <pybind11/pybind11.h>
namespace py = pybind11;

class BusyWaitSemaphore {
public:
  using value_type = std::atomic_int_fast32_t::value_type;

  BusyWaitSemaphore(const py::handle &m, bool init)
      : _view(), _p(*GetBufferPointer(m.ptr(), &_view)) {
    if (init)
      _p.store(0);
  }

  ~BusyWaitSemaphore() { Close(); }

  void Acquire() {
    auto v = _p.load();
    do {
      if (v > 0) {
        if (_p.compare_exchange_weak(v, v - 1))
          return;
      } else {
        if (PyErr_CheckSignals() != 0)
          throw py::error_already_set();
        v = _p.load();
      }
    } while (true);
  }

  void AcquireWithoutGIL() {
    PyThreadState *_save = PyEval_SaveThread();
    auto v = _p.load();
    do {
      if (v > 0) {
        if (_p.compare_exchange_weak(v, v - 1)) {
          PyEval_RestoreThread(_save);
          return;
        }
      } else {
        PyEval_RestoreThread(_save);
        if (PyErr_CheckSignals() != 0)
          throw py::error_already_set();
        _save = PyEval_SaveThread();
        v = _p.load();
      }
    } while (true);
  }

  void Release() { _p.fetch_add(1); }

  void Close() {
    if (_view.obj) {
      PyBuffer_Release(&_view);
      _view.obj = nullptr;
    }
  }

  static size_t GetRequiredMemorySize() { return sizeof(value_type); }

private:
  Py_buffer _view;
  std::atomic_ref<value_type> _p;

  static value_type DUMMY;
  static value_type *GetBufferPointer(PyObject *obj, Py_buffer *view) {
    if (PyObject_GetBuffer(obj, view, PyBUF_SIMPLE | PyBUF_WRITABLE) < 0)
      return &DUMMY;
    return static_cast<value_type *>(view->buf);
  }
};

BusyWaitSemaphore::value_type BusyWaitSemaphore::DUMMY;

class PosixSemaphore {
  sem_t *_handle;
  std::string _name;

public:
  PosixSemaphore(const std::string &name, int oflag, unsigned int value)
      : _handle(SEM_FAILED), _name(name) {
    _handle = sem_open(name.c_str(), oflag, 0660, value);
    if (_handle == SEM_FAILED)
      throw std::runtime_error("sem_open failed");
  }

  ~PosixSemaphore() { Close(); }

  void Acquire() {
    if (sem_wait(_handle) != 0)
      throw std::runtime_error("sem_wait failed");
  }

  void Release() {
    if (sem_post(_handle) != 0)
      throw std::runtime_error("sem_post failed");
  }

  void Close() {
    if (_handle == SEM_FAILED)
      return;
    sem_close(_handle);
    _handle = SEM_FAILED;
  }

  void Unlink() {
    if (_name.empty())
      return;
    sem_unlink(_name.c_str());
    _name.assign("");
  }
};

PYBIND11_MODULE(ipc_semaphore, m, py::mod_gil_not_used()) {
  py::class_<BusyWaitSemaphore>(m, "BusyWaitSemaphore")
      .def(py::init<const py::handle &, bool>())
      .def("acquire", &BusyWaitSemaphore::Acquire)
      .def("acquire_without_gil", &BusyWaitSemaphore::AcquireWithoutGIL)
      .def("release", &BusyWaitSemaphore::Release)
      .def("close", &BusyWaitSemaphore::Close)
      .def("get_required_memory_size",
           &BusyWaitSemaphore::GetRequiredMemorySize);

  py::class_<PosixSemaphore>(m, "PosixSemaphore")
      .def(py::init<const std::string &, int, unsigned int>())
      .def("acquire", &PosixSemaphore::Acquire,
           py::call_guard<py::gil_scoped_release>())
      .def("release", &PosixSemaphore::Release)
      .def("close", &PosixSemaphore::Close)
      .def("unlink", &PosixSemaphore::Unlink);

  m.attr("O_CREAT") = py::int_(O_CREAT);
  m.attr("O_EXCL") = py::int_(O_EXCL);
}
