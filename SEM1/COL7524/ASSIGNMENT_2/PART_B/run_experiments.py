#!/usr/bin/env python3

import subprocess
import time
import os
import glob
import shutil
import random
import threading
from concurrent.futures import ThreadPoolExecutor
import sys

class LoadBalancerExperiment:
    def __init__(self):
        self.base_dir = os.getcwd()
        self.experiment_data = []
        
    def cleanup(self):
        """Clean up all generated files"""
        print("🧹 Cleaning up previous files...")
        
        # Remove log files
        for pattern in ['*.log', '*.csv', 'health_log_*.csv', 'forward_log_*.csv', 
                       'server_*.log', 'metrics_*.log', 'requests_*.log']:
            for file in glob.glob(pattern):
                try:
                    os.remove(file)
                except:
                    pass
        
        # Remove test files
        for pattern in ['testfile*.txt', 'downloaded_*.txt', 'experiment_*.txt']:
            for file in glob.glob(pattern):
                try:
                    os.remove(file)
                except:
                    pass
        
        # Remove server storage directories
        for dir in glob.glob('server_storage_*'):
            try:
                shutil.rmtree(dir)
            except:
                pass
        
        # Remove plot directories
        if os.path.exists('plots'):
            shutil.rmtree('plots')
        
        # Kill any running processes
        self.kill_processes()
        time.sleep(2)
    
    def kill_processes(self):
        """Kill any running backend servers or load balancers"""
        try:
            subprocess.run(['pkill', '-f', 'backend_server'], capture_output=True)
            subprocess.run(['pkill', '-f', 'load_balancer'], capture_output=True)
            subprocess.run(['pkill', '-f', 'client'], capture_output=True)
        except:
            pass
    
    def build_system(self):
        """Build the entire system"""
        print("🔨 Building system...")
        result = subprocess.run(['make', 'clean'], capture_output=True, text=True)
        result = subprocess.run(['make'], capture_output=True, text=True)
        
        if result.returncode != 0:
            print("❌ Build failed!")
            sys.exit(1)
        print("✅ Build successful!")
    
    def start_servers(self, server_count=4):
        """Start backend servers"""
        print(f"🚀 Starting {server_count} backend servers...")
        server_processes = []
        
        for port in range(9001, 9001 + server_count):
            cmd = ['./backend_server', '127.0.0.1', str(port)]
            process = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            server_processes.append(process)
            time.sleep(0.5)  # Stagger server startup
        
        time.sleep(3)  # Wait for servers to fully start
        return server_processes
    
    def start_load_balancer(self, algorithm):
        """Start load balancer with specified algorithm"""
        print(f"🔀 Starting load balancer with {algorithm} algorithm...")
        cmd = ['./load_balancer', '--algorithm', algorithm]
        process = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(3)  # Wait for LB to start
        return process
    
    def generate_test_files(self, file_count=10, size_variation=True):
        """Generate test files with varying sizes"""
        print(f"📁 Generating {file_count} test files...")
        files = []
        
        for i in range(1, file_count + 1):
            filename = f"testfile_{i:03d}.txt"
            
            if size_variation:
                # Generate files of different sizes
                if i % 3 == 0:
                    content = f"This is a large test file {i}\n" + "x" * random.randint(1000, 5000)
                elif i % 3 == 1:
                    content = f"This is a medium test file {i}\n" + "y" * random.randint(100, 500)
                else:
                    content = f"This is a small test file {i}\n"
            else:
                content = f"This is test file {i}\n" + "data" * random.randint(1, 10)
            
            with open(filename, 'w') as f:
                f.write(content)
            files.append(filename)
        
        return files
    
    def run_client_operation(self, operation, filename, delay=0):
        """Run a single client operation"""
        time.sleep(delay)  # Add delay for realistic load
        cmd = ['./client', operation, filename]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            return False
    
    def run_workload(self, workload_type, files, clients=5):
        """Run different types of workloads"""
        print(f"💻 Running {workload_type} workload with {clients} concurrent clients...")
        
        operations = []
        
        if workload_type == "mixed":
            # Mixed PUT and GET operations
            for i, file in enumerate(files):
                if i % 3 == 0:  # PUT operation
                    operations.append(('PUT', file, random.uniform(0.1, 0.5)))
                else:  # GET operation (for files that were PUT earlier)
                    put_files = [f for op, f, _ in operations if op == 'PUT']
                    if put_files:
                        operations.append(('GET', random.choice(put_files), random.uniform(0.1, 0.3)))
        
        elif workload_type == "write_heavy":
            # Mostly PUT operations
            for file in files:
                operations.append(('PUT', file, random.uniform(0.1, 0.3)))
        
        elif workload_type == "read_heavy":
            # First PUT all files, then GET them
            for file in files:
                operations.append(('PUT', file, random.uniform(0.1, 0.3)))
            for file in files[:len(files)//2]:  # GET half of the files
                operations.append(('GET', file, random.uniform(0.05, 0.2)))
        
        elif workload_type == "burst":
            # Burst of operations
            for file in files:
                operations.append(('PUT', file, 0))  # No delay for burst
        
        # Run operations concurrently
        with ThreadPoolExecutor(max_workers=clients) as executor:
            futures = []
            for op, file, delay in operations:
                future = executor.submit(self.run_client_operation, op, file, delay)
                futures.append(future)
            
            # Wait for all operations to complete
            results = [future.result() for future in futures]
        
        success_rate = sum(results) / len(results) * 100
        print(f"✅ Workload completed: {success_rate:.1f}% success rate")
        return success_rate
    
    def simulate_server_failure(self, server_ports, failure_count=1):
        """Simulate server failures by killing some servers"""
        print(f"💥 Simulating {failure_count} server failure(s)...")
        
        ports_to_kill = random.sample(server_ports, failure_count)
        for port in ports_to_kill:
            subprocess.run(['pkill', '-f', f'backend_server.*{port}'])
            print(f"   Killed server on port {port}")
        
        time.sleep(2)  # Wait for health checks to detect failure
    
    def restart_servers(self, server_ports):
        """Restart failed servers"""
        print("🔄 Restarting failed servers...")
        for port in server_ports:
            cmd = ['./backend_server', '127.0.0.1', str(port)]
            subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(0.5)
        
        time.sleep(3)  # Wait for servers to start
    
    def run_single_experiment(self, algorithm, workload_type, file_count=20, 
                            client_count=8, duration=60, simulate_failure=False):
        """Run a single experiment with given parameters"""
        print(f"\n{'='*60}")
        print(f"🔬 EXPERIMENT: {algorithm.upper()} + {workload_type.upper()}")
        print(f"{'='*60}")
        
        experiment_id = f"{algorithm}_{workload_type}_{int(time.time())}"
        
        # Start servers
        server_ports = list(range(9001, 9005))
        server_processes = self.start_servers(len(server_ports))
        
        # Start load balancer
        lb_process = self.start_load_balancer(algorithm)
        
        # Generate test files
        files = self.generate_test_files(file_count)
        
        # Let system stabilize
        time.sleep(5)
        
        # Run workload
        start_time = time.time()
        success_rate = self.run_workload(workload_type, files, client_count)
        
        # Simulate server failure if requested
        if simulate_failure and workload_type != "burst":
            time.sleep(10)
            self.simulate_server_failure(server_ports, failure_count=1)
            
            # Continue workload after failure
            time.sleep(5)
            additional_files = self.generate_test_files(5)
            success_rate_after_failure = self.run_workload("mixed", additional_files, client_count//2)
            
            # Restart servers
            self.restart_servers(server_ports)
        
        # Run for specified duration
        elapsed = time.time() - start_time
        if elapsed < duration:
            time.sleep(duration - elapsed)
        
        # Record experiment data
        experiment_data = {
            'id': experiment_id,
            'algorithm': algorithm,
            'workload_type': workload_type,
            'file_count': file_count,
            'client_count': client_count,
            'duration': duration,
            'success_rate': success_rate,
            'simulated_failure': simulate_failure
        }
        
        self.experiment_data.append(experiment_data)
        
        # Cleanup processes
        lb_process.terminate()
        for process in server_processes:
            process.terminate()
        
        time.sleep(2)
        
        return experiment_data
    
    def run_comprehensive_experiments(self):
        """Run all experiment combinations"""
        algorithms = ['round_robin', 'least_connections']
        workload_types = ['mixed', 'write_heavy', 'read_heavy', 'burst']
        
        print("🎯 Starting comprehensive experiments...")
        print(f"Algorithms: {algorithms}")
        print(f"Workloads: {workload_types}")
        
        all_results = []
        
        # Test 1: Basic functionality with different workloads
        print("\n📊 PHASE 1: Basic workload testing")
        for algorithm in algorithms:
            for workload in workload_types:
                result = self.run_single_experiment(
                    algorithm=algorithm,
                    workload_type=workload,
                    file_count=15,
                    client_count=6,
                    duration=45,
                    simulate_failure=False
                )
                all_results.append(result)
        
        # Test 2: High load scenarios
        print("\n🔥 PHASE 2: High load testing")
        for algorithm in algorithms:
            result = self.run_single_experiment(
                algorithm=algorithm,
                workload_type='mixed',
                file_count=30,
                client_count=12,
                duration=60,
                simulate_failure=False
            )
            all_results.append(result)
        
        # Test 3: Failure recovery scenarios
        print("\n⚡ PHASE 3: Failure recovery testing")
        for algorithm in algorithms:
            result = self.run_single_experiment(
                algorithm=algorithm,
                workload_type='mixed',
                file_count=20,
                client_count=8,
                duration=90,
                simulate_failure=True
            )
            all_results.append(result)
        
        # Test 4: Different client counts
        print("\n👥 PHASE 4: Varying client counts")
        for client_count in [4, 8, 16]:
            for algorithm in algorithms:
                result = self.run_single_experiment(
                    algorithm=algorithm,
                    workload_type='mixed',
                    file_count=20,
                    client_count=client_count,
                    duration=40,
                    simulate_failure=False
                )
                all_results.append(result)
        
        return all_results
    
    def save_experiment_summary(self, results):
        """Save experiment summary to file"""
        print("\n💾 Saving experiment summary...")
        
        with open('experiment_summary.txt', 'w') as f:
            f.write("LOAD BALANCER EXPERIMENT SUMMARY\n")
            f.write("=" * 50 + "\n\n")
            
            for result in results:
                f.write(f"Experiment: {result['id']}\n")
                f.write(f"Algorithm: {result['algorithm']}\n")
                f.write(f"Workload: {result['workload_type']}\n")
                f.write(f"Files: {result['file_count']}, Clients: {result['client_count']}\n")
                f.write(f"Duration: {result['duration']}s\n")
                f.write(f"Success Rate: {result['success_rate']:.1f}%\n")
                f.write(f"Simulated Failure: {result['simulated_failure']}\n")
                f.write("-" * 30 + "\n")
        
        # Also save as CSV for easier analysis
        import pandas as pd
        df = pd.DataFrame(results)
        df.to_csv('experiment_results.csv', index=False)
        
        print("✅ Experiment summary saved!")
    
    def run_analysis(self):
        """Run analysis on collected log data"""
        print("\n📈 Running analysis on collected data...")
        
        # Check if analysis script exists
        if os.path.exists('analyze_logs.py'):
            result = subprocess.run([sys.executable, 'analyze_logs.py'], 
                                  capture_output=True, text=True)
            if result.returncode == 0:
                print("✅ Analysis completed successfully!")
            else:
                print("❌ Analysis failed!")
                print(result.stderr)
        else:
            print("⚠️  Analysis script not found, skipping analysis")
    
    def main(self):
        """Main experiment runner"""
        print("🚀 LOAD BALANCER COMPREHENSIVE EXPERIMENTS")
        print("=" * 50)
        
        try:
            # Initial cleanup
            self.cleanup()
            
            # Build system
            self.build_system()
            
            # Run comprehensive experiments
            results = self.run_comprehensive_experiments()
            
            # Save results
            self.save_experiment_summary(results)
            
            # Run analysis
            self.run_analysis()
            
            print(f"\n🎉 EXPERIMENTS COMPLETED SUCCESSFULLY!")
            print(f"📊 Generated {len(results)} experiment configurations")
            print(f"📁 Log files available for analysis")
            print(f"📈 Check 'plots' directory for generated graphs")
            
        except KeyboardInterrupt:
            print("\n⏹️  Experiments interrupted by user")
        except Exception as e:
            print(f"\n❌ Experiment failed: {e}")
        finally:
            # Final cleanup (optional - comment out if you want to keep logs)
            print("\n🧹 Final cleanup...")
            self.cleanup()
            self.kill_processes()

if __name__ == "__main__":
    experiment = LoadBalancerExperiment()
    experiment.main()