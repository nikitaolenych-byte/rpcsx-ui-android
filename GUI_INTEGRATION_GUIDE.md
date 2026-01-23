# GUI Button Control Integration Guide

## Overview

The RPCSX PS3 emulator now includes interactive GUI button controls for the RSX Graphics Engine and performance benchmarking system. This guide shows how to integrate the controls into your Android UI.

## Button Controls

### 1. RSX Start Button

**Purpose**: Start the RSX Graphics Engine from GUI  
**Function**: `RPCSX.rsxStart()`  
**Return**: `true` if started, `false` if already running

```java
// In your Activity/Fragment
Button startButton = findViewById(R.id.rsx_start_button);
startButton.setOnClickListener(v -> {
    boolean started = RPCSX.rsxStart();
    if (started) {
        Toast.makeText(this, "RSX Graphics Engine started", Toast.LENGTH_SHORT).show();
        updateUIState(true);  // Show "Running" indicator
    } else {
        Toast.makeText(this, "RSX already running or init failed", Toast.LENGTH_SHORT).show();
    }
});
```

### 2. RSX Stop Button

**Purpose**: Stop the RSX Graphics Engine  
**Function**: `RPCSX.rsxStop()`  
**Return**: void

```java
Button stopButton = findViewById(R.id.rsx_stop_button);
stopButton.setOnClickListener(v -> {
    RPCSX.rsxStop();
    Toast.makeText(this, "RSX Graphics Engine stopped", Toast.LENGTH_SHORT).show();
    updateUIState(false);  // Show "Stopped" indicator
});
```

### 3. RSX Status Check

**Purpose**: Check if RSX is currently running  
**Function**: `RPCSX.isRSXRunning()`  
**Return**: `true` if running, `false` otherwise

```java
// Update UI state periodically
private void updateRSXStatus() {
    boolean running = RPCSX.isRSXRunning();
    TextView statusText = findViewById(R.id.rsx_status);
    statusText.setText(running ? "RSX: RUNNING" : "RSX: STOPPED");
    statusText.setTextColor(running ? Color.GREEN : Color.GRAY);
}
```

## Performance Benchmarking

### Run Full Benchmark Suite

```java
Button benchmarkButton = findViewById(R.id.benchmark_button);
benchmarkButton.setOnClickListener(v -> {
    RPCSXBenchmark benchmark = new RPCSXBenchmark(rpcsx);
    
    benchmark.setBenchmarkListener(new RPCSXBenchmark.BenchmarkListener() {
        @Override
        public void onBenchmarkProgress(String testName, int progress) {
            // Update progress bar
            ProgressBar progressBar = findViewById(R.id.benchmark_progress);
            progressBar.setProgress(progress);
            
            TextView progressText = findViewById(R.id.benchmark_status);
            progressText.setText(String.format("%s: %d%%", testName, progress));
        }
        
        @Override
        public void onBenchmarkComplete(RPCSXBenchmark.BenchmarkResult result) {
            // Display results
            displayBenchmarkResults(result);
        }
        
        @Override
        public void onBenchmarkError(String error) {
            Toast.makeText(MainActivity.this, "Benchmark failed: " + error, Toast.LENGTH_LONG).show();
        }
    });
    
    benchmark.runFullBenchmark();
});
```

### Display Benchmark Results

```java
private void displayBenchmarkResults(RPCSXBenchmark.BenchmarkResult result) {
    // Results layout
    View resultsView = findViewById(R.id.benchmark_results);
    resultsView.setVisibility(View.VISIBLE);
    
    // GPU Score
    TextView gpuScore = findViewById(R.id.gpu_score);
    gpuScore.setText(String.format("GPU: %.1f FPS", result.gpuScore));
    
    // Memory Score
    TextView memoryScore = findViewById(R.id.memory_score);
    memoryScore.setText(String.format("Memory: %.1f GB/s", result.memoryScore));
    
    // CPU Score
    TextView cpuScore = findViewById(R.id.cpu_score);
    cpuScore.setText(String.format("CPU: %.1f GFLOPS", result.cpuScore));
    
    // Overall Score
    TextView overallScore = findViewById(R.id.overall_score);
    overallScore.setText(String.format("Overall: %.1f", result.overallScore));
    overallScore.setTextColor(getScoreColor(result.overallScore));
    
    // Benchmark time
    TextView timeText = findViewById(R.id.benchmark_time);
    timeText.setText(String.format("Time: %d ms", result.totalTime));
    
    // Device info
    RPCSXBenchmark.DeviceInfo info = RPCSXBenchmark.DeviceInfo.getSystemInfo();
    TextView deviceInfo = findViewById(R.id.device_info);
    deviceInfo.setText(String.format("%s - %d cores @ %.1f GHz",
        info.deviceName, info.numCores, info.cpuFrequency));
}

// Color code for score
private int getScoreColor(float score) {
    if (score >= 80) return Color.GREEN;      // Flagship
    if (score >= 70) return Color.YELLOW;     // High-End
    if (score >= 50) return Color.YELLOW;     // Mid-Range
    return Color.RED;                         // Budget
}
```

## Layout Example

```xml
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical"
    android:padding="16dp">
    
    <!-- RSX Control Section -->
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="vertical"
        android:background="#333333"
        android:padding="12dp"
        android:layout_marginBottom="16dp">
        
        <TextView
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:text="RSX Graphics Engine"
            android:textSize="16sp"
            android:textStyle="bold" />
        
        <LinearLayout
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:orientation="horizontal"
            android:layout_marginTop="8dp">
            
            <Button
                android:id="@+id/rsx_start_button"
                android:layout_width="0dp"
                android:layout_height="wrap_content"
                android:layout_weight="1"
                android:text="Start RSX"
                android:layout_marginEnd="8dp" />
            
            <Button
                android:id="@+id/rsx_stop_button"
                android:layout_width="0dp"
                android:layout_height="wrap_content"
                android:layout_weight="1"
                android:text="Stop RSX"
                android:layout_marginStart="8dp" />
        </LinearLayout>
        
        <TextView
            android:id="@+id/rsx_status"
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:text="RSX: STOPPED"
            android:layout_marginTop="8dp"
            android:textColor="#888888" />
    </LinearLayout>
    
    <!-- Benchmark Section -->
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="vertical"
        android:background="#333333"
        android:padding="12dp">
        
        <TextView
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:text="Performance Benchmark"
            android:textSize="16sp"
            android:textStyle="bold" />
        
        <Button
            android:id="@+id/benchmark_button"
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:text="Run Full Benchmark"
            android:layout_marginTop="8dp" />
        
        <ProgressBar
            android:id="@+id/benchmark_progress"
            android:layout_width="match_parent"
            android:layout_height="8dp"
            android:layout_marginTop="12dp"
            android:progress="0"
            style="@style/Widget.AppCompat.ProgressBar.Horizontal" />
        
        <TextView
            android:id="@+id/benchmark_status"
            android:layout_width="wrap_content"
            android:layout_height="wrap_content"
            android:text="Ready"
            android:layout_marginTop="8dp"
            android:textSize="12sp" />
        
        <!-- Results -->
        <LinearLayout
            android:id="@+id/benchmark_results"
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:orientation="vertical"
            android:visibility="gone"
            android:layout_marginTop="16dp"
            android:background="#1a1a1a"
            android:padding="12dp">
            
            <TextView
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Benchmark Results:"
                android:textStyle="bold"
                android:layout_marginBottom="8dp" />
            
            <TextView
                android:id="@+id/gpu_score"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="GPU: 0 FPS" />
            
            <TextView
                android:id="@+id/memory_score"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Memory: 0 GB/s" />
            
            <TextView
                android:id="@+id/cpu_score"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="CPU: 0 GFLOPS" />
            
            <TextView
                android:id="@+id/overall_score"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Overall: 0"
                android:textStyle="bold"
                android:textSize="18sp"
                android:layout_marginTop="8dp" />
            
            <TextView
                android:id="@+id/benchmark_time"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Time: 0 ms"
                android:layout_marginTop="8dp"
                android:textSize="12sp" />
            
            <TextView
                android:id="@+id/device_info"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Device: unknown"
                android:layout_marginTop="8dp"
                android:textSize="12sp" />
        </LinearLayout>
    </LinearLayout>
</LinearLayout>
```

## Best Practices

1. **Disable buttons during benchmark**
   ```java
   benchmarkButton.setEnabled(false);
   // ... run benchmark ...
   benchmarkButton.setEnabled(true);
   ```

2. **Show appropriate UI states**
   ```java
   // During benchmark
   progressBar.setVisibility(View.VISIBLE);
   stopButton.setEnabled(false);
   
   // After benchmark
   progressBar.setVisibility(View.GONE);
   stopButton.setEnabled(true);
   ```

3. **Handle background execution**
   ```java
   // Benchmark runs in background thread
   // Always update UI on main thread
   runOnUiThread(() -> {
       // Update UI
   });
   ```

4. **Store results**
   ```java
   // Save benchmark results for comparison
   SharedPreferences prefs = getSharedPreferences("benchmark", MODE_PRIVATE);
   prefs.edit()
       .putFloat("last_overall_score", result.overallScore)
       .putLong("last_benchmark_time", System.currentTimeMillis())
       .apply();
   ```

## Performance Considerations

- Benchmark takes ~1 minute to complete (30s GPU + 5s Memory + 10s CPU + 15s NCE)
- Close background apps before benchmarking
- Device should be plugged in for consistent performance
- Disable CPU frequency scaling during tests
- Thermal throttling may affect results

## Troubleshooting

| Issue | Solution |
|-------|----------|
| RSX won't start | Check if Vulkan 1.3 available and device not throttled |
| Benchmark crashes | Ensure sufficient RAM (4GB+), close background apps |
| Low GPU score | Update GPU drivers, disable battery saver mode |
| Low CPU score | Disable background processes, set CPU governor to "performance" |
| Low memory score | Ensure device is cool, close memory-hungry apps |

---

**GUI Button Integration Guide for RPCSX Performance Benchmark**
