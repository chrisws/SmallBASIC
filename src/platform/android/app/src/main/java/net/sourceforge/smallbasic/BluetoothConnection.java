package net.sourceforge.smallbasic;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresPermission;
import androidx.core.app.ActivityCompat;

import java.io.IOException;
import java.util.UUID;

/**
 * Bluetooth (non BLE) communications
 */
public class BluetoothConnection extends BroadcastReceiver {
  private static final String TAG = "smallbasic";
  private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
  private static final int CONNECT_PERMISSION = 1000;

  private final BluetoothAdapter _bluetoothAdapter;
  private final Context _context;
  private final String _deviceName;
  private BluetoothDevice _device;
  private BluetoothThread _bluetoothThread;
  private boolean _error;

  public BluetoothConnection(Activity activity, String deviceName) throws IOException {
    this._context = activity;
    this._deviceName = deviceName;
    this._bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    this._error = false;
    if (_bluetoothAdapter == null) {
      throw createIoException(activity, R.string.BLUETOOTH_ERROR);
    }

    if (checkPermission(activity, Manifest.permission.BLUETOOTH_CONNECT)) {
      requestPermission(activity, Manifest.permission.BLUETOOTH_CONNECT);
      throw createIoException(activity, R.string.PERMISSION_ERROR);
    }

    if (checkPermission(activity, Manifest.permission.BLUETOOTH_SCAN)) {
      requestPermission(activity, Manifest.permission.BLUETOOTH_SCAN);
      throw createIoException(activity, R.string.PERMISSION_ERROR);
    }

    if (!_bluetoothAdapter.isEnabled()) {
      Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
      activity.startActivity(enableBtIntent);
    }

    // Start discovery of nearby Bluetooth devices
    IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
    activity.registerReceiver(this, filter);
    _bluetoothAdapter.startDiscovery();
  }

  /**
   * Closes the USB connection
   */
  public void close() {
    Log.d(TAG, "close BT connection");
    unregisterReceiver();
    _bluetoothAdapter.cancelDiscovery();
    if (_bluetoothThread != null) {
      _bluetoothThread.stopThread();
      _bluetoothThread = null;
    }
    _device = null;
    Log.d(TAG, "BT connection closed");
  }

  /**
   * Returns information about the connected USB device
   */
  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  public String getDescription() {
    String result;
    if (_bluetoothThread != null) {
      result = _bluetoothThread.getDescription();
    } else {
      result = String.format("Local: %s [%s] Waiting for %s",
                             _bluetoothAdapter.getName(),
                             _bluetoothAdapter.getAddress(),
                             _deviceName);
    }
    return result;
  }

  /**
   * Returns whether the bluetooth connection is open
   */
  public boolean isConnected() {
    return _bluetoothThread != null && _bluetoothThread.isConnected();
  }

  /**
   * Whether a connection error has occurred
   */
  public boolean isError() {
    return _error || (_bluetoothThread != null && !_bluetoothThread.isRunning());
  }

  /**
   * Handles receiving the request permission response event
   */
  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  @Override
  public void onReceive(Context context, Intent intent) {
    Log.d(TAG, "onReceive entered");
    String action = intent.getAction();
    if (BluetoothDevice.ACTION_FOUND.equals(action)) {
      BluetoothDevice discoveredDevice = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
      if (discoveredDevice != null) {
        String deviceName = discoveredDevice.getName();
        String deviceAddress = discoveredDevice.getAddress();
        Log.d(TAG, "Found device: " + deviceName + " at " + deviceAddress);
        if (_deviceName.equals(deviceName)) {
          _device = discoveredDevice;
          unregisterReceiver();
          connectToDevice();
        }
      }
    }
  }

  /**
   * Receives the next packet of data from the usb connection
   */
  public String receive() {
    String result;
    if (_bluetoothThread != null) {
      result = _bluetoothThread.read();
    } else {
      result = "";
    }
    return result;
  }

  /**
   * Sends the given data to the usb connection
   */
  public boolean send(String data) {
    boolean result;
    if (_bluetoothThread != null) {
      result = _bluetoothThread.send(data);
    } else {
      result = false;
    }
    return result;
  }

  /**
   * Returns whether the given permission is granted
   */
  private boolean checkPermission(Activity activity, String permission) {
    return ActivityCompat.checkSelfPermission(activity, permission) != PackageManager.PERMISSION_GRANTED;
  }

  /**
   * Connects to the target device and commences communication
   */
  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  private void connectToDevice() {
    if (_device != null) {
      try {
        _bluetoothAdapter.cancelDiscovery();
        _bluetoothThread = new BluetoothThread(_device.createRfcommSocketToServiceRecord(SPP_UUID));
        _bluetoothThread.start();
        Log.d(TAG, "Connected to device: " + _device.getName());
      } catch (Exception e) {
        _error = true;
        Log.e(TAG, "Connection failed", e);
      }
    }
  }

  /**
   * Builds an Exception with the given resource string
   */
  @NonNull
  private static IOException createIoException(Context context, int resourceId) {
    return new IOException(context.getResources().getString(resourceId));
  }

  /**
   * Invokes the display of a permission prompt
   */
  private void requestPermission(Activity activity, String permission) {
    new Handler(Looper.getMainLooper()).post(() -> {
      String[] permissions = {permission};
      ActivityCompat.requestPermissions(activity, permissions, CONNECT_PERMISSION);
    });
    Log.d(TAG, "requesting permission");
  }

  /**
   * Disconnects our BroadcastReceiver listener
   */
  private void unregisterReceiver() {
    try {
      _context.unregisterReceiver(this);
    } catch (IllegalArgumentException e) {
      // ignored
    }
  }
}
