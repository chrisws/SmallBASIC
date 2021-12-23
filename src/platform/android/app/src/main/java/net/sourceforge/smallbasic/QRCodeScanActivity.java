package net.sourceforge.smallbasic;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.android.gms.vision.CameraSource;
import com.google.android.gms.vision.Detector;
import com.google.android.gms.vision.MultiProcessor;
import com.google.android.gms.vision.Tracker;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import java.io.IOException;

/**
 * QRCodeScanActivity
 *
 * Support for command "code = android.qrcode()"
 */
public class QRCodeScanActivity extends Activity implements MultiProcessor.Factory<Barcode> {
  private static final int REQUEST_CAMERA_PERMISSION = 3;
  private CameraSource _cameraSource;
  private boolean _delayedStart;

  @Override
  public Tracker<Barcode> create(Barcode barcode) {
    final QRCodeScanActivity activity = this;
    return new Tracker<Barcode>() {
      @Override
      public void onUpdate(Detector.Detections<Barcode> detections, Barcode barcode) {
        Log.i(Consts.TAG, "barcode detected");
        try {
          final boolean[] started = new boolean[] { false };
          runOnUiThread(new Runnable() {
            public void run() {
              if (!started[0]) {
                started[0] = true;
                _cameraSource.stop();
                Log.i(Consts.TAG, "return to calling activity");
                Intent intent = new Intent(activity, MainActivity.class);
                intent.putExtra(Consts.QR_CODE_EXTRA_KEY, barcode.displayValue);
                setResult(Consts.QRCODE_RESULT_CODE, intent);
                finish();
              }
            }
          });
        } catch (Exception e) {
          Log.i(Consts.TAG, "invalid game barcode");
        }
      }
    };
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.qrcode_scan);
    Log.i(Consts.TAG, "onCreate entered");
    BarcodeDetector detector = new BarcodeDetector.Builder(this).setBarcodeFormats(Barcode.QR_CODE).build();
    detector.setProcessor(new MultiProcessor.Builder<>(this).build());
    _delayedStart = false;
    _cameraSource = new CameraSource.Builder(this, detector).setFacing(CameraSource.CAMERA_FACING_BACK).build();
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    if (requestCode == REQUEST_CAMERA_PERMISSION && grantResults[0] == PackageManager.PERMISSION_DENIED) {
      onStop();
      startActivity(new Intent(this, MainActivity.class));
    }
  }

  @SuppressLint("MissingPermission")
  @Override
  protected void onResume() {
    super.onResume();
    if (isPermitted()) {
      Log.i(Consts.TAG, "start camera");
      setupCamera();
    }
  }

  @Override
  protected void onStart() {
    super.onStart();
    if (!isPermitted()) {
      _delayedStart = true;
      showPermissionDialog();
    }
  }

  private boolean isPermitted() {
    return (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED);
  }

  private void setupCamera() {
    final SurfaceView surfaceView = this.findViewById(R.id.surfaceView);
    surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
      @Override
      public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height) {
        // ignored
      }

      @Override
      public void surfaceCreated(SurfaceHolder surfaceHolder) {
        startCamera(surfaceHolder);
      }

      @Override
      public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
        Log.i(Consts.TAG, "surfaceDestroyed");
        _cameraSource.stop();
      }
    });
    if (_delayedStart) {
      startCamera(surfaceView.getHolder());
    }
  }

  private void showPermissionDialog() {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        String[] permissions = new String[] {Manifest.permission.CAMERA};
        ActivityCompat.requestPermissions(QRCodeScanActivity.this, permissions, REQUEST_CAMERA_PERMISSION);
      }
    });
  }

  @SuppressLint("MissingPermission")
  private void startCamera(SurfaceHolder surfaceHolder) {
    try {
      _cameraSource.start(surfaceHolder);
    } catch (IOException ie) {
      Log.e("camera error: ", ie.getMessage());
    }
  }
}
