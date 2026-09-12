# avx2_opt 配置：默认目标机 = .66 VM (Intel, callgrind 真值机)
# 注意：callgrind 是软件计数，x86-64 二进制在 Intel/AMD 上 Ir 一致，
#       故在 .66 上测得的 Ir 直接对 AMD EPYC 7B13 方向有效（仅 cycles 不同）。

VM_HOST = "192.168.1.66"
VM_USER = "azzr"
VM_KEY  = "~/.ssh/id_ed25519"
VM_WORKDIR = "/tmp/avx2_opt"   # VM 上工作区（/tmp 重启即丢，需重建）

# 本地构建产物也允许（若本机有 g++ + valgrind）；默认走 VM。
LOCAL_FALLBACK = False

# callgrind 参数
CALLGRIND_EXTRA = "--cache-sim=no --branch-sim=no"  # 只取 Ir（指令条数），最快
