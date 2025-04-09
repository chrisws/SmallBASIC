package net.sourceforge.smallbasic;

import android.Manifest;
import android.bluetooth.BluetoothSocket;
import android.util.Log;

import androidx.annotation.RequiresPermission;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Thread for sending and receiving data on a bluetooth connection
 */
public class BluetoothThread extends Thread {
  private static final String TAG = "smallbasic";
  private static final int RECEIVE_BUFFER_SIZE = 1024;
  private static final int RING_BUFFER_SIZE = 4096;
  private final BluetoothSocket _socket;
  private final BlockingQueue<byte[]> _sendQueue;
  private final RingBuffer _ringBuffer;
  private final AtomicBoolean _running;

  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  public BluetoothThread(BluetoothSocket socket) throws IOException {
    this._socket = socket;
    this._ringBuffer = new RingBuffer(RING_BUFFER_SIZE);
    this._sendQueue = new LinkedBlockingQueue<>();
    this._running = new AtomicBoolean(true);
    socket.connect();
  }

  @RequiresPermission(Manifest.permission.BLUETOOTH_CONNECT)
  public String getDescription() {
    return String.format("Remote %s [%s]",
                         _socket.getRemoteDevice().getName(),
                         _socket.getRemoteDevice().getAddress());
  }

  public boolean isConnected() {
    return isAlive() && _socket.isConnected();
  }

  public boolean isRunning() {
    return _running.get();
  }

  public String read() {
    return _ringBuffer.read();
  }

  @Override
  public void run() {
    try {
      InputStream inputStream = _socket.getInputStream();
      OutputStream outputStream = _socket.getOutputStream();
      while (_running.get() && !Thread.currentThread().isInterrupted()) {
        send(outputStream);
        receive(inputStream);
        sleep();
      }
    } catch (Exception e) {
      Log.d(TAG, "Run failed:", e);
    } finally {
      _running.set(false);
      closeSocket();
    }
    Log.d(TAG, "Bluetooth thread terminating");
  }

  /**
   * Add data to the send queue. This will block if the queue is full
   */
  public boolean send(String data) {
    boolean result;
    try {
      _sendQueue.put(data.getBytes(StandardCharsets.UTF_8));
      result = true;
    } catch (InterruptedException e) {
      Log.d(TAG, "Send failed:", e);
      result = false;
    }
    return result;
  }

  public void stopThread() {
    _running.set(false);
    closeSocket();
    interrupt();
    Log.d(TAG, "stopThread invoked");
  }

  /**
   * Close the socket at termination of the thread
   */
  private void closeSocket() {
    try {
      _socket.close();
      Log.d(TAG, "BT socket closed.");
    }
    catch (IOException e) {
      Log.e(TAG, "Error closing socket", e);
    }
  }

  /**
   * Read data from Bluetooth input stream into ring buffer
   */
  private void receive(InputStream inputStream) throws IOException {
    byte[] buffer = new byte[RECEIVE_BUFFER_SIZE];
    int bytesRead = inputStream.read(buffer);
    if (bytesRead > 0) {
      _ringBuffer.write(buffer, bytesRead);
    }
  }

  /**
   * Send data from the queue to Bluetooth output stream
   */
  private void send(OutputStream outputStream) throws IOException {
    byte[] data = _sendQueue.poll();
    if (data != null) {
      outputStream.write(data);
      outputStream.flush();
    }
  }

  /**
   * Sleep or short period to avoid busy-waiting
   */
  private static void sleep() {
    try {
      Thread.sleep(10);
    } catch (InterruptedException e) {
      // ignored
    }
  }
}
