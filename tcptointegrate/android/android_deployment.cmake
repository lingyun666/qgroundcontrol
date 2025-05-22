# Android部署设置

# 设置Android SDK版本
set(ANDROID_API_VERSION 30)
set(ANDROID_SDK_VERSION 30)

# 设置最小SDK版本
set(ANDROID_MIN_SDK_VERSION 23)

# 设置应用名称和版本
set(ANDROID_APP_NAME "TCP文件传输")
set(ANDROID_APP_VERSION ${PROJECT_VERSION})
set(ANDROID_APP_VERSION_CODE 1)

# 设置权限
set(ANDROID_PERMISSIONS
    "android.permission.INTERNET"
    "android.permission.WRITE_EXTERNAL_STORAGE"
    "android.permission.READ_EXTERNAL_STORAGE"
)

# 设置屏幕方向
set(ANDROID_SCREEN_ORIENTATION "unspecified")

# APK构建设置
set(ANDROID_APK_DEBUGGABLE TRUE)

# 设置应用图标
set(ANDROID_ICON "android/res/drawable/icon.png") 