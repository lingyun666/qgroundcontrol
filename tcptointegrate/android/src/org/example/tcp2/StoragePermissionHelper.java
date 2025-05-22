package org.example.tcp2;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;

public class StoragePermissionHelper {
    private static final String TAG = "StoragePermissionHelper";

    /**
     * 请求管理外部存储权限（适用于Android 11及以上）
     * @param context 应用上下文
     */
    public static void requestManageExternalStoragePermission(Context context) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Log.i(TAG, "请求Android 11+的外部存储管理权限");
                if (!Environment.isExternalStorageManager()) {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + context.getPackageName()));
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    context.startActivity(intent);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "打开权限设置失败", e);
            try {
                // 尝试打开常规的应用权限设置页面
                Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                intent.setData(Uri.parse("package:" + context.getPackageName()));
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                context.startActivity(intent);
            } catch (Exception ex) {
                Log.e(TAG, "打开应用设置也失败", ex);
            }
        }
    }
} 