"""Python install script for the basler-video-capture Python module.
"""
import os
import sys
import subprocess as sub

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    """Custom setuptools extension a CMake compiled library.
    """
    def __init__(self, name, sourcedir=''):
        # super(name, sources=[])
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    """Custom build process for compiling a CMake library.
    """
    def run(self):
        try:
            sub.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError(
                ('CMake must be installed to build the following extensions: '
                '{}').format(', '.join(e.name for e in self.extensions)))

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        """Build the given extension.
        """
        # Setting up build variables
        extdir = os.path.abspath(
                os.path.dirname(self.get_ext_fullpath(ext.name)))
        cfg = 'Debug' if self.debug else 'Release'
        cmake_cmd = ['cmake', ext.sourcedir,
                     '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
                     '-DPYTHON_EXECUTABLE=' + sys.executable,
                     '-DCMAKE_BUILD_TYPE=' + cfg]
        build_cmd = ['cmake', '--build', '.', '--config', cfg, '--', '-j2']
        env = os.environ.copy()
        env['CXXFLAGS'] = '{} -DVERSION_INFO=\\"{}\\"'.format(
                env.get('CXXFLAGS', ''),
                self.distribution.get_version())

        # Creating build directory if it does not exist
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        
        # Executing cmake
        sub.check_call(cmake_cmd, cwd=self.build_temp, env=env)

        # Compiling project
        sub.check_call(build_cmd, cwd=self.build_temp)
        print()  # Adding print for cleaner output


setup(
    name='basler_video_capture',
    version='0.1',
    author='Kevin Midkiff',
    author_email='kevin.midkiff@intel.com',
    description='Simple Pylon 5 Python wrapper',
    long_description=('Pylon 5 wrapper exposing VideoCapture object for '
                 'Basler GigE cameras'),
    ext_modules=[CMakeExtension('src')],
    cmdclass=dict(build_ext=CMakeBuild),
    scripts=['bin/basler-capture'],
    test_suite='tests',
    zip_safe=False)

