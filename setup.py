from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup

module_name = "ipc_semaphore"
__version__ = "0.0.1"

ext_modules = [
    Pybind11Extension(
        module_name,
        [
            "src/main.cpp",
        ],
        define_macros=[("VERSION_INFO", __version__)],
        cxx_std=20,
    ),
]

setup(
    name=module_name,
    version=__version__,
    author="Kazuki Oikawa",
    author_email="k@oikw.org",
    description=module_name.replace("_", " "),
    long_description="",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.9",
)
