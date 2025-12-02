# Wallet Fix for OxygenOS

使OxygenOS的**一加12**可以使用Oppo钱包，若使用其他设备，请在安装完成后创建`/data/adb/walletfix/spoof_vars`文件，文件内容为`MODEL=你的设备型号`

目前仅对钱包应用和银联可信服务安全组件伪装机型，如有需要，可创建`/data/adb/walletfix/target.txt`文件，内容为需要伪装的应用包名列表，每行一个包名，至少需包函`com.finshell.wallet`和`com.unionpay.tsmservice`

若不知道对应国行版本的设备型号，可以从 [这里](https://github.com/KHwang9883/MobileModels/blob/master/brands/oneplus.md) 查找

例如一加12国行型号位`PJD110`，则文件内容为`MODEL=PJD110`

已在一加12的OxygenOS 14和OxygenOS15测试

## 说明

本模块使用从 一加12 ColorOS 的全量OTA包中提取的 账号中心 和 OpenID 替换系统中的对应应用，然后对钱包应用伪装为国行系统

## 注意

本模块会替换 账号应用 为国行版本，**介意请勿安装**

## 使用方法

1. 安装Oppo钱包
2. 安装银联可信服务安全组件（如果不需要支付功能可以跳过）
3. 确保已安装 Magisk/KernelSU/Apatch，并已启用 zygisk
4. 安装本模块
5. 若设备不是一加12，参考上文最开头部分

## 已知问题

1. eid 不工作 (已找到原因及解决方法，见https://github.com/geniucker-dev/WalletFix4OOS/issues/13)

## License

[GPL-3.0](LICENSE)

## Acknowledgements

- [PlayIntegrityFix](https://github.com/chiteroman/PlayIntegrityFix)
- [TrickyStore](https://github.com/5ec1cff/TrickyStore/tree/master)
