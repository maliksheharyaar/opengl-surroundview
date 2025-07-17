#!/usr/bin/env python3
"""
SurroundView3D Performance Profiler
Launches the application and monitors system performance in real-time
"""

import sys
import os
import subprocess
import threading
import time
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
import psutil
import GPUtil
from collections import deque
import numpy as np
import csv
from datetime import datetime

class SurroundViewProfiler:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("SurroundView3D Performance Profiler")
        self.root.geometry("1200x800")
        self.root.configure(bg='#2b2b2b')
        
        # Performance data storage
        self.max_data_points = 100
        self.timestamps = deque(maxlen=self.max_data_points)
        self.cpu_usage = deque(maxlen=self.max_data_points)
        self.ram_usage = deque(maxlen=self.max_data_points)
        self.gpu_usage = deque(maxlen=self.max_data_points)
        self.vram_usage = deque(maxlen=self.max_data_points)
        self.fps_data = deque(maxlen=self.max_data_points)
        
        # Application process
        self.app_process = None
        self.monitoring = False
        self.start_time = time.time()
        
        # CSV replay mode
        self.replay_mode = False
        self.replay_data = []
        self.replay_index = 0
        self.replay_timer = None
        
        # Data recording
        self.recording_data = []
        self.session_start_time = None
        
        # Setup GUI
        self.setup_gui()
        
        # Performance monitoring thread
        self.monitor_thread = None
        
    def setup_gui(self):
        """Setup the GUI layout with controls and charts"""
        
        # Main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Control panel
        control_frame = ttk.LabelFrame(main_frame, text="Application Control", padding=10)
        control_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Launch button
        self.launch_btn = ttk.Button(control_frame, text="Launch SurroundView3D", 
                                    command=self.launch_application, style="Accent.TButton")
        self.launch_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        # Stop button
        self.stop_btn = ttk.Button(control_frame, text="Stop Application", 
                                  command=self.stop_application, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        # Export CSV button
        self.export_btn = ttk.Button(control_frame, text="Export CSV", 
                                    command=self.export_csv, state=tk.DISABLED)
        self.export_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        # Load CSV button
        self.load_btn = ttk.Button(control_frame, text="Load CSV", 
                                  command=self.load_csv)
        self.load_btn.pack(side=tk.LEFT, padx=(0, 10))
        
        # Replay controls frame
        replay_frame = ttk.Frame(control_frame)
        replay_frame.pack(side=tk.LEFT, padx=(20, 0))
        
        self.replay_btn = ttk.Button(replay_frame, text="Play Replay", 
                                    command=self.start_replay, state=tk.DISABLED)
        self.replay_btn.pack(side=tk.LEFT, padx=(0, 5))
        
        self.stop_replay_btn = ttk.Button(replay_frame, text="Stop Replay", 
                                         command=self.stop_replay, state=tk.DISABLED)
        self.stop_replay_btn.pack(side=tk.LEFT)
        
        # Status label
        self.status_label = ttk.Label(control_frame, text="Status: Ready to launch")
        self.status_label.pack(side=tk.LEFT, padx=(20, 0))
        
        # Recording indicator
        self.recording_label = ttk.Label(control_frame, text="", foreground="red")
        self.recording_label.pack(side=tk.LEFT, padx=(10, 0))
        
        # Performance metrics frame
        metrics_frame = ttk.LabelFrame(main_frame, text="Real-time Metrics", padding=10)
        metrics_frame.pack(fill=tk.X, pady=(0, 10))
        
        # Create metrics display
        metrics_grid = ttk.Frame(metrics_frame)
        metrics_grid.pack(fill=tk.X)
        
        # CPU metrics
        cpu_frame = ttk.Frame(metrics_grid)
        cpu_frame.grid(row=0, column=0, padx=(0, 20), sticky="w")
        ttk.Label(cpu_frame, text="CPU Usage:", font=("Arial", 10, "bold")).pack()
        self.cpu_label = ttk.Label(cpu_frame, text="0.0%", font=("Arial", 12))
        self.cpu_label.pack()
        
        # RAM metrics
        ram_frame = ttk.Frame(metrics_grid)
        ram_frame.grid(row=0, column=1, padx=(0, 20), sticky="w")
        ttk.Label(ram_frame, text="RAM Usage:", font=("Arial", 10, "bold")).pack()
        self.ram_label = ttk.Label(ram_frame, text="0.0 GB", font=("Arial", 12))
        self.ram_label.pack()
        
        # GPU metrics
        gpu_frame = ttk.Frame(metrics_grid)
        gpu_frame.grid(row=0, column=2, padx=(0, 20), sticky="w")
        ttk.Label(gpu_frame, text="GPU Usage:", font=("Arial", 10, "bold")).pack()
        self.gpu_label = ttk.Label(gpu_frame, text="0.0%", font=("Arial", 12))
        self.gpu_label.pack()
        
        # VRAM metrics
        vram_frame = ttk.Frame(metrics_grid)
        vram_frame.grid(row=0, column=3, padx=(0, 20), sticky="w")
        ttk.Label(vram_frame, text="VRAM Usage:", font=("Arial", 10, "bold")).pack()
        self.vram_label = ttk.Label(vram_frame, text="0.0 GB", font=("Arial", 12))
        self.vram_label.pack()
        
        # FPS metrics
        fps_frame = ttk.Frame(metrics_grid)
        fps_frame.grid(row=0, column=4, sticky="w")
        ttk.Label(fps_frame, text="Est. FPS:", font=("Arial", 10, "bold")).pack()
        self.fps_label = ttk.Label(fps_frame, text="0.0", font=("Arial", 12))
        self.fps_label.pack()
        
        # Charts frame
        charts_frame = ttk.LabelFrame(main_frame, text="Performance Charts", padding=10)
        charts_frame.pack(fill=tk.BOTH, expand=True)
        
        # Setup matplotlib figure
        self.fig, ((self.ax1, self.ax2), (self.ax3, self.ax4)) = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.patch.set_facecolor('#2b2b2b')
        
        # Style the plots
        for ax in [self.ax1, self.ax2, self.ax3, self.ax4]:
            ax.set_facecolor('#1e1e1e')
            ax.tick_params(colors='white')
            ax.spines['bottom'].set_color('white')
            ax.spines['top'].set_color('white')
            ax.spines['right'].set_color('white')
            ax.spines['left'].set_color('white')
        
        # Configure subplots
        self.ax1.set_title("CPU Usage (%)", color='white')
        self.ax1.set_ylim(0, 100)
        
        self.ax2.set_title("RAM Usage (GB)", color='white')
        
        self.ax3.set_title("GPU Usage (%)", color='white')
        self.ax3.set_ylim(0, 100)
        
        self.ax4.set_title("Estimated FPS", color='white')
        self.ax4.set_ylim(0, 20)  # Reduced from 60 to 20 since we never reach high FPS
        
        # Embed matplotlib in tkinter
        canvas = FigureCanvasTkAgg(self.fig, charts_frame)
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        # Animation for real-time updates
        self.animation = FuncAnimation(self.fig, self.update_charts, interval=1000, blit=False, cache_frame_data=False)
        
    def launch_application(self):
        """Launch the SurroundView3D application"""
        try:
            exe_path = os.path.join("build", "Release", "SurroundView3D.exe")
            if not os.path.exists(exe_path):
                self.status_label.config(text="Error: SurroundView3D.exe not found in build/Release/")
                return
            
            # Launch the application
            self.app_process = subprocess.Popen([exe_path], 
                                              cwd=os.getcwd(),
                                              creationflags=subprocess.CREATE_NEW_CONSOLE)
            
            # Update UI
            self.launch_btn.config(state=tk.DISABLED)
            self.stop_btn.config(state=tk.NORMAL)
            self.status_label.config(text="Status: Application running - monitoring performance...")
            
            # Start monitoring
            self.monitoring = True
            self.start_time = time.time()
            self.session_start_time = datetime.now()
            self.recording_data = []  # Clear previous recording
            self.recording_label.config(text="● REC", foreground="red")
            self.monitor_thread = threading.Thread(target=self.monitor_performance, daemon=True)
            self.monitor_thread.start()
            
        except Exception as e:
            self.status_label.config(text=f"Error launching application: {str(e)}")
    
    def stop_application(self):
        """Stop the application and monitoring"""
        if self.app_process:
            self.app_process.terminate()
            self.app_process = None
        
        self.monitoring = False
        
        # Update UI
        self.launch_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self.export_btn.config(state=tk.NORMAL if self.recording_data else tk.DISABLED)
        self.recording_label.config(text="")
        self.status_label.config(text="Status: Application stopped")
    
    def monitor_performance(self):
        """Monitor system performance in background thread"""
        frame_count = 0
        last_frame_time = time.time()
        
        while self.monitoring:
            try:
                current_time = time.time() - self.start_time
                
                # Get CPU usage
                cpu_percent = psutil.cpu_percent(interval=0.1)
                
                # Get RAM usage
                memory = psutil.virtual_memory()
                ram_gb = memory.used / (1024**3)
                
                # Get GPU usage (if available)
                gpu_percent = 0
                vram_gb = 0
                try:
                    gpus = GPUtil.getGPUs()
                    if gpus:
                        gpu = gpus[0]  # Use first GPU
                        gpu_percent = gpu.load * 100
                        vram_gb = gpu.memoryUsed / 1024  # Convert MB to GB
                except:
                    pass  # GPU monitoring may not be available
                
                # Estimate FPS (very rough based on monitoring frequency)
                frame_count += 1
                current_frame_time = time.time()
                time_diff = current_frame_time - last_frame_time
                if time_diff >= 1.0:  # Update FPS every second
                    estimated_fps = frame_count / time_diff
                    frame_count = 0
                    last_frame_time = current_frame_time
                else:
                    # Use last known FPS if we haven't hit 1 second yet
                    estimated_fps = len(self.fps_data) and self.fps_data[-1] or 0
                
                # Store data
                self.timestamps.append(current_time)
                self.cpu_usage.append(cpu_percent)
                self.ram_usage.append(ram_gb)
                self.gpu_usage.append(gpu_percent)
                self.vram_usage.append(vram_gb)
                self.fps_data.append(min(estimated_fps, 20))  # Cap at 20 for display
                
                # Record data for CSV export
                self.recording_data.append({
                    'timestamp': current_time,
                    'cpu_usage': cpu_percent,
                    'ram_usage': ram_gb,
                    'gpu_usage': gpu_percent,
                    'vram_usage': vram_gb,
                    'fps': estimated_fps
                })
                
                # Update labels in main thread
                self.root.after(0, self.update_labels, cpu_percent, ram_gb, gpu_percent, vram_gb, estimated_fps)
                
                time.sleep(0.5)  # Monitor every 500ms
                
            except Exception as e:
                print(f"Monitoring error: {e}")
                break
    
    def update_labels(self, cpu, ram, gpu, vram, fps):
        """Update the metric labels"""
        self.cpu_label.config(text=f"{cpu:.1f}%")
        self.ram_label.config(text=f"{ram:.2f} GB")
        self.gpu_label.config(text=f"{gpu:.1f}%")
        self.vram_label.config(text=f"{vram:.2f} GB")
        self.fps_label.config(text=f"{fps:.1f}")
    
    def export_csv(self):
        """Export performance data to CSV file"""
        if not self.recording_data:
            messagebox.showwarning("No Data", "No performance data to export")
            return
        
        # Generate default filename with timestamp
        if self.session_start_time:
            default_name = f"benchmark_{self.session_start_time.strftime('%Y%m%d_%H%M%S')}.csv"
        else:
            default_name = f"benchmark_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        
        filename = filedialog.asksaveasfilename(
            defaultextension=".csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
            initialfile=default_name
        )
        
        if filename:
            try:
                with open(filename, 'w', newline='') as csvfile:
                    fieldnames = ['timestamp', 'cpu_usage', 'ram_usage', 'gpu_usage', 'vram_usage', 'fps']
                    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                    
                    # Write header
                    writer.writeheader()
                    
                    # Write data
                    for data_point in self.recording_data:
                        writer.writerow(data_point)
                
                messagebox.showinfo("Export Successful", f"Benchmark data exported to:\n{filename}")
                
            except Exception as e:
                messagebox.showerror("Export Failed", f"Failed to export data:\n{str(e)}")
    
    def load_csv(self):
        """Load performance data from CSV file for replay"""
        filename = filedialog.askopenfilename(
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")]
        )
        
        if filename:
            try:
                self.replay_data = []
                with open(filename, 'r') as csvfile:
                    reader = csv.DictReader(csvfile)
                    for row in reader:
                        self.replay_data.append({
                            'timestamp': float(row['timestamp']),
                            'cpu_usage': float(row['cpu_usage']),
                            'ram_usage': float(row['ram_usage']),
                            'gpu_usage': float(row['gpu_usage']),
                            'vram_usage': float(row['vram_usage']),
                            'fps': float(row['fps'])
                        })
                
                if self.replay_data:
                    self.replay_btn.config(state=tk.NORMAL)
                    self.status_label.config(text=f"Status: Loaded {len(self.replay_data)} data points for replay")
                    messagebox.showinfo("Load Successful", f"Loaded {len(self.replay_data)} data points\nReady for replay")
                else:
                    messagebox.showwarning("No Data", "No valid data found in CSV file")
                    
            except Exception as e:
                messagebox.showerror("Load Failed", f"Failed to load CSV file:\n{str(e)}")
    
    def start_replay(self):
        """Start replaying loaded CSV data"""
        if not self.replay_data:
            messagebox.showwarning("No Data", "No replay data loaded")
            return
        
        self.replay_mode = True
        self.replay_index = 0
        self.start_time = time.time()
        
        # Clear current data
        self.timestamps.clear()
        self.cpu_usage.clear()
        self.ram_usage.clear()
        self.gpu_usage.clear()
        self.vram_usage.clear()
        self.fps_data.clear()
        
        # Update UI
        self.replay_btn.config(state=tk.DISABLED)
        self.stop_replay_btn.config(state=tk.NORMAL)
        self.launch_btn.config(state=tk.DISABLED)
        self.load_btn.config(state=tk.DISABLED)
        self.status_label.config(text="Status: Replaying benchmark data...")
        
        # Start replay timer
        self.replay_next_point()
    
    def stop_replay(self):
        """Stop the replay"""
        self.replay_mode = False
        if self.replay_timer:
            self.root.after_cancel(self.replay_timer)
            self.replay_timer = None
        
        # Update UI
        self.replay_btn.config(state=tk.NORMAL if self.replay_data else tk.DISABLED)
        self.stop_replay_btn.config(state=tk.DISABLED)
        self.launch_btn.config(state=tk.NORMAL)
        self.load_btn.config(state=tk.NORMAL)
        self.status_label.config(text="Status: Replay stopped")
    
    def replay_next_point(self):
        """Replay the next data point"""
        if not self.replay_mode or self.replay_index >= len(self.replay_data):
            if self.replay_index >= len(self.replay_data):
                self.status_label.config(text="Status: Replay completed")
            self.stop_replay()
            return
        
        data_point = self.replay_data[self.replay_index]
        
        # Add data to current display
        self.timestamps.append(data_point['timestamp'])
        self.cpu_usage.append(data_point['cpu_usage'])
        self.ram_usage.append(data_point['ram_usage'])
        self.gpu_usage.append(data_point['gpu_usage'])
        self.vram_usage.append(data_point['vram_usage'])
        self.fps_data.append(min(data_point['fps'], 20))  # Cap at 20 for display
        
        # Update labels
        self.update_labels(
            data_point['cpu_usage'],
            data_point['ram_usage'],
            data_point['gpu_usage'],
            data_point['vram_usage'],
            data_point['fps']
        )
        
        self.replay_index += 1
        
        # Schedule next point (replay at 2x speed for quicker viewing)
        next_delay = 250  # 250ms between points (2x faster than recording)
        self.replay_timer = self.root.after(next_delay, self.replay_next_point)
    
    def update_charts(self, frame):
        """Update the performance charts"""
        if not self.timestamps:
            return
        
        # Clear previous plots
        self.ax1.clear()
        self.ax2.clear()
        self.ax3.clear()
        self.ax4.clear()
        
        # Convert timestamps to relative seconds
        times = np.array(list(self.timestamps))
        
        # Plot CPU usage
        self.ax1.plot(times, list(self.cpu_usage), 'cyan', linewidth=2)
        self.ax1.set_title("CPU Usage (%)", color='white')
        self.ax1.set_ylim(0, 100)
        self.ax1.grid(True, alpha=0.3)
        self.ax1.set_facecolor('#1e1e1e')
        
        # Add annotation for latest CPU value
        if self.cpu_usage:
            latest_cpu = self.cpu_usage[-1]
            latest_time = times[-1]
            self.ax1.annotate(f'{latest_cpu:.1f}%', 
                             xy=(latest_time, latest_cpu), 
                             xytext=(10, 10), textcoords='offset points',
                             bbox=dict(boxstyle='round,pad=0.3', facecolor='cyan', alpha=0.7),
                             color='black', fontweight='bold', fontsize=9)
        
        # Plot RAM usage
        self.ax2.plot(times, list(self.ram_usage), 'yellow', linewidth=2)
        self.ax2.set_title("RAM Usage (GB)", color='white')
        self.ax2.grid(True, alpha=0.3)
        self.ax2.set_facecolor('#1e1e1e')
        
        # Add annotation for latest RAM value
        if self.ram_usage:
            latest_ram = self.ram_usage[-1]
            latest_time = times[-1]
            self.ax2.annotate(f'{latest_ram:.2f} GB', 
                             xy=(latest_time, latest_ram), 
                             xytext=(10, 10), textcoords='offset points',
                             bbox=dict(boxstyle='round,pad=0.3', facecolor='yellow', alpha=0.7),
                             color='black', fontweight='bold', fontsize=9)
        
        # Plot GPU usage
        self.ax3.plot(times, list(self.gpu_usage), 'green', linewidth=2)
        self.ax3.set_title("GPU Usage (%)", color='white')
        self.ax3.set_ylim(0, 100)
        self.ax3.grid(True, alpha=0.3)
        self.ax3.set_facecolor('#1e1e1e')
        
        # Add annotation for latest GPU value
        if self.gpu_usage:
            latest_gpu = self.gpu_usage[-1]
            latest_time = times[-1]
            self.ax3.annotate(f'{latest_gpu:.1f}%', 
                             xy=(latest_time, latest_gpu), 
                             xytext=(10, 10), textcoords='offset points',
                             bbox=dict(boxstyle='round,pad=0.3', facecolor='green', alpha=0.7),
                             color='black', fontweight='bold', fontsize=9)
        
        # Plot FPS
        self.ax4.plot(times, list(self.fps_data), 'red', linewidth=2)
        self.ax4.set_title("Estimated FPS", color='white')
        self.ax4.set_ylim(0, 20)  # Reduced from 60 to 20 since we never reach high FPS
        self.ax4.grid(True, alpha=0.3)
        self.ax4.set_facecolor('#1e1e1e')
        
        # Add annotation for latest FPS value
        if self.fps_data:
            latest_fps = self.fps_data[-1]
            latest_time = times[-1]
            self.ax4.annotate(f'{latest_fps:.1f} FPS', 
                             xy=(latest_time, latest_fps), 
                             xytext=(10, 10), textcoords='offset points',
                             bbox=dict(boxstyle='round,pad=0.3', facecolor='red', alpha=0.7),
                             color='white', fontweight='bold', fontsize=9)
        
        # Style all axes
        for ax in [self.ax1, self.ax2, self.ax3, self.ax4]:
            ax.tick_params(colors='white')
            for spine in ax.spines.values():
                spine.set_color('white')
        
        plt.tight_layout()
    
    def on_closing(self):
        """Handle window closing"""
        self.stop_application()
        self.stop_replay()
        self.root.destroy()
    
    def run(self):
        """Run the profiler"""
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.root.mainloop()

def main():
    """Main function"""
    print("SurroundView3D Performance Profiler")
    print("====================================")
    
    # Check if required packages are available
    try:
        import psutil
        import GPUtil
        import matplotlib
        print("✓ All required packages available")
    except ImportError as e:
        print(f"✗ Missing required package: {e}")
        print("Please install: pip install psutil GPUtil matplotlib")
        sys.exit(1)
    
    # Check if executable exists
    exe_path = os.path.join("build", "Release", "SurroundView3D.exe")
    if not os.path.exists(exe_path):
        print(f"✗ SurroundView3D.exe not found at: {exe_path}")
        print("Please build the project first using the build script")
        sys.exit(1)
    
    print("✓ SurroundView3D.exe found")
    print("Starting profiler...")
    
    # Create and run profiler
    profiler = SurroundViewProfiler()
    profiler.run()

if __name__ == "__main__":
    main()
