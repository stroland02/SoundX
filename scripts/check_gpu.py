"""Smoke test: verify PyTorch sees the GPU and can run a CUDA op."""

import torch


def main() -> None:
    print(f"PyTorch:        {torch.__version__}")
    print(f"CUDA available: {torch.cuda.is_available()}")
    if not torch.cuda.is_available():
        raise SystemExit("CUDA is not available - check driver / torch build")

    device = torch.device("cuda")
    print(f"Device:         {torch.cuda.get_device_name(device)}")
    print(f"CUDA runtime:   {torch.version.cuda}")
    capability = torch.cuda.get_device_capability(device)
    print(f"Compute cap:    sm_{capability[0]}{capability[1]}")

    # Run a real kernel so we know compute works, not just detection.
    a = torch.randn(2048, 2048, device=device)
    b = torch.randn(2048, 2048, device=device)
    c = a @ b
    torch.cuda.synchronize()
    print(f"Matmul OK:      result shape {tuple(c.shape)}, mean {c.mean().item():.4f}")


if __name__ == "__main__":
    main()
