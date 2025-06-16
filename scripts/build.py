#!/usr/bin/env python3
"""
Build script for FastFileSearch project.
Handles building, testing, and security scanning.
"""

import os
import sys
import subprocess
import argparse
import shutil
from pathlib import Path
from typing import List, Optional

class BuildSystem:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.build_dir = project_root / 'build'
        self.install_dir = project_root / 'install'
    
    def clean(self) -> bool:
        """Clean build directories."""
        print("Cleaning build directories...")
        
        try:
            if self.build_dir.exists():
                shutil.rmtree(self.build_dir)
            if self.install_dir.exists():
                shutil.rmtree(self.install_dir)
            print("Clean completed successfully.")
            return True
        except Exception as e:
            print(f"Error during clean: {e}")
            return False
    
    def configure(self, build_type: str = 'Release', enable_tests: bool = True) -> bool:
        """Configure the build with CMake."""
        print(f"Configuring build (type: {build_type})...")
        
        try:
            self.build_dir.mkdir(exist_ok=True)
            
            cmake_args = [
                'cmake',
                f'-DCMAKE_BUILD_TYPE={build_type}',
                f'-DCMAKE_INSTALL_PREFIX={self.install_dir}',
                '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',  # For clang-tidy
            ]
            
            if enable_tests:
                cmake_args.append('-DBUILD_TESTING=ON')
            
            cmake_args.append(str(self.project_root))
            
            result = subprocess.run(cmake_args, cwd=self.build_dir, check=True)
            print("Configuration completed successfully.")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Configuration failed: {e}")
            return False
        except Exception as e:
            print(f"Error during configuration: {e}")
            return False
    
    def build(self, target: Optional[str] = None, parallel_jobs: int = 0) -> bool:
        """Build the project."""
        print("Building project...")
        
        try:
            if not self.build_dir.exists():
                print("Build directory doesn't exist. Run configure first.")
                return False
            
            cmake_args = ['cmake', '--build', str(self.build_dir)]
            
            if target:
                cmake_args.extend(['--target', target])
            
            if parallel_jobs > 0:
                cmake_args.extend(['--parallel', str(parallel_jobs)])
            elif parallel_jobs == 0:
                # Use all available cores
                import multiprocessing
                cmake_args.extend(['--parallel', str(multiprocessing.cpu_count())])
            
            result = subprocess.run(cmake_args, check=True)
            print("Build completed successfully.")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Build failed: {e}")
            return False
        except Exception as e:
            print(f"Error during build: {e}")
            return False
    
    def test(self, test_filter: Optional[str] = None) -> bool:
        """Run tests."""
        print("Running tests...")
        
        try:
            if not (self.build_dir / 'FastFileSearchTests').exists():
                print("Test executable not found. Build tests first.")
                return False
            
            ctest_args = ['ctest', '--output-on-failure', '--verbose']
            
            if test_filter:
                ctest_args.extend(['-R', test_filter])
            
            result = subprocess.run(ctest_args, cwd=self.build_dir, check=True)
            print("Tests completed successfully.")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Tests failed: {e}")
            return False
        except Exception as e:
            print(f"Error running tests: {e}")
            return False
    
    def install(self) -> bool:
        """Install the built project."""
        print("Installing project...")
        
        try:
            cmake_args = ['cmake', '--install', str(self.build_dir)]
            result = subprocess.run(cmake_args, check=True)
            print("Installation completed successfully.")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Installation failed: {e}")
            return False
        except Exception as e:
            print(f"Error during installation: {e}")
            return False
    
    def package(self) -> bool:
        """Create distribution packages."""
        print("Creating packages...")
        
        try:
            # Build packages using CPack
            cpack_args = ['cpack']
            result = subprocess.run(cpack_args, cwd=self.build_dir, check=True)
            print("Packaging completed successfully.")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Packaging failed: {e}")
            return False
        except Exception as e:
            print(f"Error during packaging: {e}")
            return False
    
    def run_security_scan(self) -> bool:
        """Run security scanning."""
        print("Running security scan...")
        
        try:
            security_script = self.project_root / 'scripts' / 'security_scan.py'
            if not security_script.exists():
                print("Security scan script not found.")
                return False
            
            scan_args = [
                sys.executable,
                str(security_script),
                '--project-root', str(self.project_root),
                '--output', str(self.build_dir / 'security_report.txt')
            ]
            
            result = subprocess.run(scan_args, check=True)
            print("Security scan completed successfully.")
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"Security scan failed: {e}")
            return False
        except Exception as e:
            print(f"Error during security scan: {e}")
            return False
    
    def run_static_analysis(self) -> bool:
        """Run static analysis tools."""
        print("Running static analysis...")
        
        success = True
        
        # Run cppcheck
        try:
            cppcheck_args = [
                'cppcheck',
                '--enable=all',
                '--error-exitcode=1',
                '--suppress=missingIncludeSystem',
                '--suppress=unusedFunction',
                str(self.project_root / 'src'),
                str(self.project_root / 'include')
            ]
            
            result = subprocess.run(cppcheck_args, check=False, capture_output=True, text=True)
            if result.returncode != 0:
                print("cppcheck found issues:")
                print(result.stderr)
                success = False
            else:
                print("cppcheck: No issues found.")
                
        except FileNotFoundError:
            print("Warning: cppcheck not found. Install with: apt-get install cppcheck")
        
        # Run clang-format check
        try:
            format_script = self.project_root / 'scripts' / 'check_format.py'
            if format_script.exists():
                result = subprocess.run([sys.executable, str(format_script)], check=False)
                if result.returncode != 0:
                    print("Code formatting issues found.")
                    success = False
                else:
                    print("Code formatting: OK")
        except Exception as e:
            print(f"Error checking code format: {e}")
        
        return success
    
    def generate_documentation(self) -> bool:
        """Generate project documentation."""
        print("Generating documentation...")
        
        try:
            # Check if Doxygen is available
            doxygen_config = self.project_root / 'docs' / 'Doxyfile'
            if not doxygen_config.exists():
                print("Doxygen configuration not found.")
                return False
            
            result = subprocess.run(['doxygen', str(doxygen_config)], 
                                  cwd=self.project_root, check=True)
            print("Documentation generated successfully.")
            return True
            
        except FileNotFoundError:
            print("Warning: Doxygen not found. Install with: apt-get install doxygen")
            return False
        except subprocess.CalledProcessError as e:
            print(f"Documentation generation failed: {e}")
            return False
        except Exception as e:
            print(f"Error generating documentation: {e}")
            return False

def main():
    parser = argparse.ArgumentParser(description='Build system for FastFileSearch')
    parser.add_argument('--project-root', type=Path, default=Path('.'),
                       help='Root directory of the project')
    parser.add_argument('--build-type', choices=['Debug', 'Release', 'RelWithDebInfo'], 
                       default='Release', help='Build type')
    parser.add_argument('--jobs', type=int, default=0,
                       help='Number of parallel build jobs (0 = auto)')
    
    # Action arguments
    parser.add_argument('--clean', action='store_true',
                       help='Clean build directories')
    parser.add_argument('--configure', action='store_true',
                       help='Configure the build')
    parser.add_argument('--build', action='store_true',
                       help='Build the project')
    parser.add_argument('--test', action='store_true',
                       help='Run tests')
    parser.add_argument('--install', action='store_true',
                       help='Install the project')
    parser.add_argument('--package', action='store_true',
                       help='Create distribution packages')
    parser.add_argument('--security-scan', action='store_true',
                       help='Run security scanning')
    parser.add_argument('--static-analysis', action='store_true',
                       help='Run static analysis')
    parser.add_argument('--docs', action='store_true',
                       help='Generate documentation')
    parser.add_argument('--all', action='store_true',
                       help='Run complete build pipeline')
    
    # Test options
    parser.add_argument('--test-filter', type=str,
                       help='Filter for running specific tests')
    parser.add_argument('--no-tests', action='store_true',
                       help='Disable building tests')
    
    args = parser.parse_args()
    
    # Validate project root
    if not args.project_root.exists():
        print(f"Error: Project root {args.project_root} does not exist")
        sys.exit(1)
    
    # Initialize build system
    build_system = BuildSystem(args.project_root)
    
    success = True
    
    # Execute requested actions
    if args.all:
        # Complete build pipeline
        actions = [
            ('clean', lambda: build_system.clean()),
            ('configure', lambda: build_system.configure(args.build_type, not args.no_tests)),
            ('build', lambda: build_system.build(parallel_jobs=args.jobs)),
            ('test', lambda: build_system.test(args.test_filter) if not args.no_tests else True),
            ('static-analysis', lambda: build_system.run_static_analysis()),
            ('security-scan', lambda: build_system.run_security_scan()),
            ('install', lambda: build_system.install()),
            ('docs', lambda: build_system.generate_documentation()),
        ]
    else:
        # Individual actions
        actions = []
        if args.clean:
            actions.append(('clean', lambda: build_system.clean()))
        if args.configure:
            actions.append(('configure', lambda: build_system.configure(args.build_type, not args.no_tests)))
        if args.build:
            actions.append(('build', lambda: build_system.build(parallel_jobs=args.jobs)))
        if args.test:
            actions.append(('test', lambda: build_system.test(args.test_filter)))
        if args.static_analysis:
            actions.append(('static-analysis', lambda: build_system.run_static_analysis()))
        if args.security_scan:
            actions.append(('security-scan', lambda: build_system.run_security_scan()))
        if args.install:
            actions.append(('install', lambda: build_system.install()))
        if args.package:
            actions.append(('package', lambda: build_system.package()))
        if args.docs:
            actions.append(('docs', lambda: build_system.generate_documentation()))
    
    # If no actions specified, default to configure and build
    if not actions:
        actions = [
            ('configure', lambda: build_system.configure(args.build_type, not args.no_tests)),
            ('build', lambda: build_system.build(parallel_jobs=args.jobs)),
        ]
    
    # Execute actions
    for action_name, action_func in actions:
        print(f"\n{'='*60}")
        print(f"Executing: {action_name}")
        print(f"{'='*60}")
        
        if not action_func():
            print(f"Action '{action_name}' failed!")
            success = False
            break
    
    # Summary
    print(f"\n{'='*60}")
    if success:
        print("✅ Build pipeline completed successfully!")
    else:
        print("❌ Build pipeline failed!")
    print(f"{'='*60}")
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
