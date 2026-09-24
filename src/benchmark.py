#!/usr/bin/env python3
import time
import numpy as np
import onnxruntime as ort

def benchmark_streaming_inference():
    onnx_path = "exported_models/ddsp_decoder.onnx"
    
    # Session-Optionen für minimale Latenz auf der CPU
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 1
    opts.inter_op_num_threads = 1
    opts.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    
    session = ort.InferenceSession(onnx_path, opts, providers=["CPUExecutionProvider"])
    
    # 1 Frame entspricht 128 Samples @ 44.1 kHz = 2.902 ms Audio-Dauer
    frame_budget_ms = (128 / 44100.0) * 1000.0
    
    f0_input = np.array([[[0.3]]], dtype=np.float32)
    loudness_input = np.array([[[0.7]]], dtype=np.float32)
    inputs = {"f0": f0_input, "loudness": loudness_input}

    # Warmup
    for _ in range(50):
        session.run(None, inputs)

    iterations = 2000
    times = []

    for _ in range(iterations):
        start = time.perf_counter()
        session.run(None, inputs)
        times.append(time.perf_counter() - start)

    times = np.array(times) * 1000.0  # in Millisekunden
    avg_time = np.mean(times)
    p99_time = np.percentile(times, 99)

    print("--- Inferenz-Benchmark (CPU, Single Frame = 128 Samples) ---")
    print(f"Echtzeit-Budget:          {frame_budget_ms:.3f} ms")
    print(f"Durchschnittliche Dauer:  {avg_time:.3f} ms")
    print(f"99. Perzentil (Worst-Case): {p99_time:.3f} ms")
    print(f"Auslastung des Budgets:   {(avg_time / frame_budget_ms) * 100:.2f} %")

    # Kriterium: Inferenz muss weniger als 20% des Audio-Budgets verbrauchen
    assert avg_time < (frame_budget_ms * 0.20), "Inferenzzeit überschreitet 20% des Latenzbudgets"
    print("Voraussetzung für Phase 4 erfüllt: Inferenz ist echtzeitfähig.")

if __name__ == "__main__":
    benchmark_streaming_inference()
