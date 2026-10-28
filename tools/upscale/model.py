"""The 2x texture network (PyTorch, training only; port/upscale/upscale.c runs it in C). docs/port/UPSCALE.md.

All on the low-resolution grid: 3x3 convolution 3->C, PReLU; N x (3x3 C->C, PReLU); plus the first layer's output
(a skip); 3x3 C->12; pixel shuffle to 3 channels at 2x; added to a Catmull-Rom 2x of the input. Every
convolution pads its input by one texel, by wrapping (a repeating texture) or replicating (clamped), separately
for each axis, as the C code does.
"""
import torch
import torch.nn as nn
import torch.nn.functional as F

CUBIC = ((-3 / 128, 29 / 128, 111 / 128, -9 / 128), (-9 / 128, 111 / 128, 29 / 128, -3 / 128))


def pad2d(x, n, wrap_s, wrap_t):
    x = F.pad(x, (n, n, 0, 0), mode="circular" if wrap_s else "replicate")
    return F.pad(x, (0, 0, n, n), mode="circular" if wrap_t else "replicate")


def cubic2x(x, wrap_s=False, wrap_t=False):
    """Catmull-Rom 2x of (B, C, H, W): output 2i at input i - 0.25, 2i+1 at i + 0.25."""
    p = pad2d(x, 2, wrap_s, wrap_t)
    h, w = x.shape[2], x.shape[3]
    def axis(t, dim, n):
        sl = lambda a: t.narrow(dim, a, n)
        p0 = sum(CUBIC[0][k] * sl(k) for k in range(4))
        p1 = sum(CUBIC[1][k] * sl(k + 1) for k in range(4))
        out = torch.stack([p0, p1], dim=dim + 1)
        shape = list(p0.shape)
        shape[dim] *= 2
        return out.reshape(shape)
    t = axis(p, 3, w)
    return axis(t, 2, h)


class Net(nn.Module):
    def __init__(self, channels=16, blocks=4):
        super().__init__()
        self.channels, self.blocks = channels, blocks
        self.head = nn.Conv2d(3, channels, 3)
        self.head_act = nn.PReLU(channels, init=0.1)
        self.body = nn.ModuleList(nn.Conv2d(channels, channels, 3) for _ in range(blocks))
        self.body_act = nn.ModuleList(nn.PReLU(channels, init=0.1) for _ in range(blocks))
        self.tail = nn.Conv2d(channels, 12, 3)
        nn.init.zeros_(self.tail.weight)   # start as plain Catmull-Rom
        nn.init.zeros_(self.tail.bias)

    def forward(self, x, wrap_s=False, wrap_t=False):
        """x: (B, 3, H, W) in [0, 1] -> (B, 3, 2H, 2W), not clamped."""
        base = cubic2x(x, wrap_s, wrap_t)
        f = self.head_act(self.head(pad2d(x - 0.5, 1, wrap_s, wrap_t)))
        h = f
        for conv, act in zip(self.body, self.body_act):
            h = act(conv(pad2d(h, 1, wrap_s, wrap_t)))
        r = self.tail(pad2d(h + f, 1, wrap_s, wrap_t))
        return base + F.pixel_shuffle(r, 2)

    def params(self):
        return sum(p.numel() for p in self.parameters())
