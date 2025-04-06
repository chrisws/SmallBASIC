package net.sourceforge.smallbasic;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresPermission;
import androidx.core.app.ActivityCompat;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.UUID;

/**
 * Bluetooth (non BLE) communications
 */
public class BluetoothConnection extends BroadcastReceiver {
  private static final String TAG = "smallbasic";
  private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
  private static final int RECEIVE_BUFFER_SIZE = 64;
  private static final int MAX_RECEIVE_SIZE = 128;
  private static final int CONNECT_PERMISSION = 1000;

  private final BluetoothAdapter _bluetoothAdapter;
  private final Context _context;
  private final String _deviceName;
  private BluetoothSocket _bluetoothSocket;
  private BluetoothDevice _device;

  /**
   * Constructs a new BluetoothConnection
   */
  public BluetoothConnection(Activity activity, String deviceName) throws IOException {
    _context = activity;
    _deviceName = deviceName;
    _bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    if (_bluetoothAdapter == null) {
      throw createIoException(activity, R.string.BLUETOOTH_ERROR);
    }

    if (ActivityCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
      requestPermission(activity, Manifest.permission.BLUETOOTH_CONNECT);
      throw createIoException(activity, R.string.PERMISSION_ERROR);
    }

    if (ActivityCompat.checkSelfPermission(activity, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
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
    try {
      unregisterReceiver();
      if (_bluetoothSocket != null) {
        _bluetoothSocket.close();
        Log.d(TAG, "Connection closed.");
      }
    } catch (Exception e) {
      Log.e(TAG, "Error closing connection", e);
    }
  }

  /**
   * Returns information about the connected USB device
   */
  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  public String getDescription() {
    String result;
    if (_bluetoothSocket != null) {
      result = String.format("Remote %s [%s]",
                             _bluetoothSocket.getRemoteDevice().getName(),
                             _bluetoothSocket.getRemoteDevice().getAddress());
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
    return _bluetoothSocket != null && _bluetoothSocket.isConnected();
  }

  /**
   * Handles receiving the request permission response event
   */
  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  @Override
  public void onReceive(Context context, Intent intent) {
    Log.d(TAG, "onReceive entered");
    String action = intent.getAction();
    if (Manifest.permission.BLUETOOTH_CONNECT.equals(action)) {
      String message = "";
      new Handler(Looper.getMainLooper()).post(() -> {
        Toast.makeText(context, message, Toast.LENGTH_LONG).show();
      });
    } else if (BluetoothDevice.ACTION_FOUND.equals(action)) {
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
    String result = null;
    try {
      InputStream dataIn = _bluetoothSocket.getInputStream();
      ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
      byte[] buffer = new byte[RECEIVE_BUFFER_SIZE];
      for (int n = dataIn.read(buffer); n != -1; n = dataIn.read(buffer)) {
        outputStream.write(buffer, 0, n);
        if (outputStream.size() > MAX_RECEIVE_SIZE) {
          // break in case the device is continuously sending
          break;
        }
      }
      result = new String(outputStream.toByteArray(), StandardCharsets.UTF_8);
    } catch (IOException e) {
      Log.e(TAG, "Error receiving data", e);
    }
    return result;
  }

  /**
   * Sends the given data to the usb connection
   */
  public int send(String data) {
    byte[] dataOut = data.getBytes(StandardCharsets.UTF_8);
    try {
      OutputStream outputStream = _bluetoothSocket.getOutputStream();
      outputStream.write(dataOut);
    } catch (IOException e) {
      Log.e(TAG, "Error sending data", e);
    }
    return 0;
  }

  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  private void connectToDevice() {
    if (_device != null) {
      try {
        _bluetoothSocket = _device.createRfcommSocketToServiceRecord(SPP_UUID);
        _bluetoothSocket.connect();
        Log.d(TAG, "Connected to device: " + _device.getName());
        // Now you can send and receive data via bluetoothSocket
      } catch (Exception e) {
        Log.e(TAG, "Connection failed", e);
      }
    }
  }

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

  private void unregisterReceiver() {
    try {
      _context.unregisterReceiver(this);
    } catch (IllegalArgumentException e) {
      // ignored
    }
  }
}
