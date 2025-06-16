#!/usr/bin/env python3
"""
Security scanning script for FastFileSearch project.
Performs static analysis, dependency scanning, and security checks.
"""

import os
import sys
import subprocess
import json
import re
import argparse
from pathlib import Path
from typing import List, Dict, Any, Optional

class SecurityScanner:
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.results = {
            'static_analysis': [],
            'dependency_scan': [],
            'code_quality': [],
            'security_issues': [],
            'summary': {}
        }
    
    def run_cppcheck(self) -> List[Dict[str, Any]]:
        """Run cppcheck static analysis."""
        print("Running cppcheck static analysis...")
        
        issues = []
        try:
            cmd = [
                'cppcheck',
                '--enable=all',
                '--xml',
                '--xml-version=2',
                '--suppress=missingIncludeSystem',
                '--suppress=unusedFunction',
                str(self.project_root / 'src'),
                str(self.project_root / 'include')
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            # Parse XML output
            import xml.etree.ElementTree as ET
            if result.stderr:
                try:
                    root = ET.fromstring(result.stderr)
                    for error in root.findall('.//error'):
                        issue = {
                            'tool': 'cppcheck',
                            'severity': error.get('severity', 'unknown'),
                            'id': error.get('id', 'unknown'),
                            'message': error.get('msg', 'No message'),
                            'file': error.get('file', 'unknown'),
                            'line': error.get('line', '0')
                        }
                        issues.append(issue)
                except ET.ParseError:
                    print(f"Warning: Could not parse cppcheck XML output")
            
        except FileNotFoundError:
            print("Warning: cppcheck not found. Install with: apt-get install cppcheck")
        except Exception as e:
            print(f"Error running cppcheck: {e}")
        
        return issues
    
    def run_clang_tidy(self) -> List[Dict[str, Any]]:
        """Run clang-tidy static analysis."""
        print("Running clang-tidy analysis...")
        
        issues = []
        try:
            # Find all C++ source files
            cpp_files = []
            for ext in ['*.cpp', '*.cxx', '*.cc']:
                cpp_files.extend(self.project_root.glob(f'src/**/{ext}'))
            
            for cpp_file in cpp_files:
                cmd = [
                    'clang-tidy',
                    str(cpp_file),
                    '--',
                    f'-I{self.project_root}/include',
                    '-std=c++20'
                ]
                
                result = subprocess.run(cmd, capture_output=True, text=True)
                
                # Parse clang-tidy output
                for line in result.stdout.split('\n'):
                    if ':' in line and ('warning:' in line or 'error:' in line):
                        parts = line.split(':')
                        if len(parts) >= 4:
                            issue = {
                                'tool': 'clang-tidy',
                                'file': parts[0],
                                'line': parts[1],
                                'column': parts[2],
                                'severity': 'warning' if 'warning:' in line else 'error',
                                'message': ':'.join(parts[3:]).strip()
                            }
                            issues.append(issue)
        
        except FileNotFoundError:
            print("Warning: clang-tidy not found. Install with: apt-get install clang-tidy")
        except Exception as e:
            print(f"Error running clang-tidy: {e}")
        
        return issues
    
    def scan_for_security_patterns(self) -> List[Dict[str, Any]]:
        """Scan for common security anti-patterns in code."""
        print("Scanning for security patterns...")
        
        issues = []
        
        # Security patterns to look for
        patterns = {
            'buffer_overflow': [
                r'strcpy\s*\(',
                r'strcat\s*\(',
                r'sprintf\s*\(',
                r'gets\s*\(',
            ],
            'sql_injection': [
                r'SELECT.*\+.*',
                r'INSERT.*\+.*',
                r'UPDATE.*\+.*',
                r'DELETE.*\+.*',
            ],
            'path_traversal': [
                r'\.\./',
                r'\.\.\\\\'
            ],
            'hardcoded_secrets': [
                r'password\s*=\s*["\'][^"\']+["\']',
                r'api_key\s*=\s*["\'][^"\']+["\']',
                r'secret\s*=\s*["\'][^"\']+["\']',
            ],
            'weak_crypto': [
                r'MD5',
                r'SHA1',
                r'DES',
                r'RC4',
            ]
        }
        
        # Scan all source files
        for file_path in self.project_root.rglob('*.cpp'):
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    
                for category, pattern_list in patterns.items():
                    for pattern in pattern_list:
                        matches = re.finditer(pattern, content, re.IGNORECASE)
                        for match in matches:
                            line_num = content[:match.start()].count('\n') + 1
                            issue = {
                                'tool': 'security_scanner',
                                'category': category,
                                'file': str(file_path.relative_to(self.project_root)),
                                'line': str(line_num),
                                'pattern': pattern,
                                'match': match.group(),
                                'severity': 'high' if category in ['buffer_overflow', 'sql_injection'] else 'medium'
                            }
                            issues.append(issue)
            
            except Exception as e:
                print(f"Error scanning {file_path}: {e}")
        
        return issues
    
    def check_dependencies(self) -> List[Dict[str, Any]]:
        """Check for known vulnerabilities in dependencies."""
        print("Checking dependencies for vulnerabilities...")
        
        issues = []
        
        # Check CMakeLists.txt for dependency versions
        cmake_file = self.project_root / 'CMakeLists.txt'
        if cmake_file.exists():
            try:
                with open(cmake_file, 'r') as f:
                    content = f.read()
                
                # Look for find_package calls
                find_package_pattern = r'find_package\s*\(\s*(\w+)(?:\s+(\d+(?:\.\d+)*))?\s*'
                matches = re.finditer(find_package_pattern, content, re.IGNORECASE)
                
                for match in matches:
                    package = match.group(1)
                    version = match.group(2) if match.group(2) else 'unspecified'
                    
                    # Check against known vulnerable versions (simplified)
                    vulnerable_packages = {
                        'Qt6': {'min_safe': '6.5.0'},
                        'SQLite3': {'min_safe': '3.40.0'},
                    }
                    
                    if package in vulnerable_packages:
                        min_safe = vulnerable_packages[package]['min_safe']
                        if version == 'unspecified':
                            issue = {
                                'tool': 'dependency_scanner',
                                'package': package,
                                'version': version,
                                'severity': 'medium',
                                'message': f'Version not specified for {package}. Recommend >= {min_safe}'
                            }
                            issues.append(issue)
            
            except Exception as e:
                print(f"Error checking dependencies: {e}")
        
        return issues
    
    def check_file_permissions(self) -> List[Dict[str, Any]]:
        """Check for overly permissive file permissions."""
        print("Checking file permissions...")
        
        issues = []
        
        try:
            for file_path in self.project_root.rglob('*'):
                if file_path.is_file():
                    stat = file_path.stat()
                    mode = oct(stat.st_mode)[-3:]
                    
                    # Check for world-writable files
                    if mode.endswith('6') or mode.endswith('7'):
                        issue = {
                            'tool': 'permission_checker',
                            'file': str(file_path.relative_to(self.project_root)),
                            'permissions': mode,
                            'severity': 'medium',
                            'message': 'File is world-writable'
                        }
                        issues.append(issue)
                    
                    # Check for executable files that shouldn't be
                    if file_path.suffix in ['.txt', '.md', '.json', '.xml'] and mode.endswith(('1', '3', '5', '7')):
                        issue = {
                            'tool': 'permission_checker',
                            'file': str(file_path.relative_to(self.project_root)),
                            'permissions': mode,
                            'severity': 'low',
                            'message': 'Non-executable file has execute permissions'
                        }
                        issues.append(issue)
        
        except Exception as e:
            print(f"Error checking file permissions: {e}")
        
        return issues
    
    def run_all_scans(self) -> Dict[str, Any]:
        """Run all security scans and compile results."""
        print("Starting comprehensive security scan...")
        
        # Run static analysis
        self.results['static_analysis'].extend(self.run_cppcheck())
        self.results['static_analysis'].extend(self.run_clang_tidy())
        
        # Run security pattern scanning
        self.results['security_issues'].extend(self.scan_for_security_patterns())
        
        # Check dependencies
        self.results['dependency_scan'].extend(self.check_dependencies())
        
        # Check file permissions
        self.results['security_issues'].extend(self.check_file_permissions())
        
        # Generate summary
        self.generate_summary()
        
        return self.results
    
    def generate_summary(self):
        """Generate a summary of all findings."""
        total_issues = 0
        severity_counts = {'high': 0, 'medium': 0, 'low': 0, 'info': 0}
        
        for category in ['static_analysis', 'security_issues', 'dependency_scan']:
            for issue in self.results[category]:
                total_issues += 1
                severity = issue.get('severity', 'info')
                if severity in severity_counts:
                    severity_counts[severity] += 1
                else:
                    severity_counts['info'] += 1
        
        self.results['summary'] = {
            'total_issues': total_issues,
            'severity_breakdown': severity_counts,
            'categories': {
                'static_analysis': len(self.results['static_analysis']),
                'security_issues': len(self.results['security_issues']),
                'dependency_scan': len(self.results['dependency_scan'])
            }
        }
    
    def generate_report(self, output_file: Optional[Path] = None) -> str:
        """Generate a human-readable security report."""
        report = []
        report.append("=" * 80)
        report.append("FastFileSearch Security Scan Report")
        report.append("=" * 80)
        report.append("")
        
        # Summary
        summary = self.results['summary']
        report.append(f"Total Issues Found: {summary['total_issues']}")
        report.append("")
        report.append("Severity Breakdown:")
        for severity, count in summary['severity_breakdown'].items():
            report.append(f"  {severity.upper()}: {count}")
        report.append("")
        
        # Detailed findings
        for category, issues in self.results.items():
            if category == 'summary' or not issues:
                continue
            
            report.append(f"{category.replace('_', ' ').title()}:")
            report.append("-" * 40)
            
            for issue in issues:
                report.append(f"  [{issue.get('severity', 'info').upper()}] {issue.get('message', 'No message')}")
                if 'file' in issue:
                    report.append(f"    File: {issue['file']}")
                if 'line' in issue:
                    report.append(f"    Line: {issue['line']}")
                report.append("")
        
        report_text = "\n".join(report)
        
        if output_file:
            with open(output_file, 'w') as f:
                f.write(report_text)
            print(f"Report saved to: {output_file}")
        
        return report_text

def main():
    parser = argparse.ArgumentParser(description='Security scanner for FastFileSearch')
    parser.add_argument('--project-root', type=Path, default=Path('.'),
                       help='Root directory of the project')
    parser.add_argument('--output', type=Path,
                       help='Output file for the report')
    parser.add_argument('--json', action='store_true',
                       help='Output results in JSON format')
    parser.add_argument('--fail-on-high', action='store_true',
                       help='Exit with error code if high severity issues found')
    
    args = parser.parse_args()
    
    # Validate project root
    if not args.project_root.exists():
        print(f"Error: Project root {args.project_root} does not exist")
        sys.exit(1)
    
    # Run security scan
    scanner = SecurityScanner(args.project_root)
    results = scanner.run_all_scans()
    
    # Output results
    if args.json:
        output = json.dumps(results, indent=2)
        if args.output:
            with open(args.output, 'w') as f:
                f.write(output)
        else:
            print(output)
    else:
        report = scanner.generate_report(args.output)
        if not args.output:
            print(report)
    
    # Check for high severity issues
    if args.fail_on_high and results['summary']['severity_breakdown']['high'] > 0:
        print(f"Error: Found {results['summary']['severity_breakdown']['high']} high severity issues")
        sys.exit(1)
    
    print(f"Security scan completed. Found {results['summary']['total_issues']} total issues.")

if __name__ == '__main__':
    main()
