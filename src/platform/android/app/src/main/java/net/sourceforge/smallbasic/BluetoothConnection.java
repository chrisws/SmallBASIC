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

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Bluetooth (non BLE) communications
 */
public class BluetoothConnection extends BroadcastReceiver {
  private static final String TAG = "smallbasic";
  private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
  private static final int CONNECT_PERMISSION = 1000;
  private static final int SEND_TIMEOUT_SECS = 10;
  private static final int RING_BUFFER_SIZE = 4096;

  private final BluetoothAdapter _bluetoothAdapter;
  private final Context _context;
  private final String _deviceName;
  private final RingBuffer _ringBuffer;
  private BluetoothSocket _bluetoothSocket;
  private BluetoothDevice _device;
  private ReceiverThread _receiverThread;

  /**
   * Constructs a new BluetoothConnection
   */
  public BluetoothConnection(Activity activity, String deviceName) throws IOException {
    this._context = activity;
    this._deviceName = deviceName;
    this._bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    this._ringBuffer = new RingBuffer(RING_BUFFER_SIZE);
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
    unregisterReceiver();
    _bluetoothAdapter.cancelDiscovery();
    if (_receiverThread != null) {
      _receiverThread.interrupt();
    }
    closeSocket();
    _device = null;
    _receiverThread = null;
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
      String message = "todo - what here?";
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
    return _ringBuffer.read();
  }

  /**
   * Sends the given data to the usb connection
   */
  public boolean send(String data) {
    final byte[] dataOut = data.getBytes(StandardCharsets.UTF_8);
    AtomicBoolean result = new AtomicBoolean(false);
    CountDownLatch latch = new CountDownLatch(1);
    new Thread(new Runnable() {
      @Override
      public void run() {
        try {
          OutputStream outputStream = _bluetoothSocket.getOutputStream();
          outputStream.write(dataOut);
          result.set(true);
        } catch (IOException e) {
          Log.e(TAG, "Error sending data", e);
        } finally {
          latch.countDown();
        }
      }
    }).start();
    endLatch(latch);
    return result.get();
  }

  private void closeSocket() {
    if (_bluetoothSocket != null) {
      try {
        _bluetoothSocket.close();
        Log.d(TAG, "Connection closed.");
      }
      catch (IOException e) {
        Log.e(TAG, "Error closing connection", e);
      }
    }
  }

  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  private void connectToDevice() {
    if (_device != null) {
      try {
        _bluetoothAdapter.cancelDiscovery();
        _bluetoothSocket = _device.createRfcommSocketToServiceRecord(SPP_UUID);
        _bluetoothSocket.connect();
        _receiverThread = new ReceiverThread(_bluetoothSocket, _ringBuffer);
        _receiverThread.start();
        Log.d(TAG, "Connected to device: " + _device.getName());
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
   * Wait for the looper to process the next consumer
   */
  private void endLatch(CountDownLatch latch) {
    try {
      if (!latch.await(SEND_TIMEOUT_SECS, TimeUnit.SECONDS)) {
        Log.d(TAG, "timeout waiting");
      }
    } catch (InterruptedException e) {
      Log.d(TAG, "failed waiting", e);
    }
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

  /**
   * Thread for receiving data
   */
  static class ReceiverThread extends Thread {
    private static final int RECEIVE_BUFFER_SIZE = 1024;
    private final BluetoothSocket _socket;
    private final RingBuffer _ringBuffer;
    private InputStream _inputStream;

    public ReceiverThread(BluetoothSocket socket, RingBuffer ringBuffer) {
      this._socket = socket;
      this._ringBuffer = ringBuffer;
      try {
        this._inputStream = socket.getInputStream();
      } catch (IOException e) {
        Log.d(TAG, "get stream failed", e);
      }
    }

    @Override
    public void run() {
      byte[] buffer = new byte[RECEIVE_BUFFER_SIZE];
      int bytesRead;

      try {
        while (!Thread.currentThread().isInterrupted()) {
          bytesRead = _inputStream.read(buffer);
          if (bytesRead > 0) {
            _ringBuffer.write(buffer, bytesRead);
          }
        }
      } catch (IOException e) {
        Log.d(TAG, "read failed", e);
      } finally {
        try {
          _socket.close();
        } catch (IOException e) {
          Log.d(TAG, "close failed", e);
        }
      }
      Log.d(TAG, "receiver thread terminating");
    }
  }

  /**
   * RingBuffer for receiving data
   */
  static class RingBuffer {
    private static final int MAX_READ_LENGTH = 80;
    private final byte[] buffer;
    private int head;
    private int tail;
    private int size;

    RingBuffer(int capacity) {
      buffer = new byte[capacity];
      head = 0;
      tail = 0;
      size = 0;
    }

    synchronized boolean hasData() {
      return size > 0;
    }

    synchronized String read() {
      int bytesToRead = Math.min(MAX_READ_LENGTH, size);
      byte[] output = new byte[bytesToRead];

      for (int i = 0; i < bytesToRead; i++) {
        output[i] = buffer[head];
        head = (head + 1) % buffer.length;
        size--;
      }

      return new String(output, StandardCharsets.UTF_8);
    }

    synchronized void write(byte[] data, int length) {
      if (length > buffer.length - size) {
        // Buffer overflow, cannot write
        return;
      }

      for (int i = 0; i < length; i++) {
        buffer[tail] = data[i];
        tail = (tail + 1) % buffer.length;
        size++;
      }
    }
  }
}
