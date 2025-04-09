package net.sourceforge.smallbasic;

import android.bluetooth.BluetoothSocket;
import android.util.Log;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Thread for sending data on a bluetooth connection
 */
public class BluetoothTxThread extends Thread {
  private static final String TAG = "smallbasic";
  private final AtomicBoolean _running;
  private final BlockingQueue<byte[]> _sendQueue;
  private final OutputStream _outputStream;

  public BluetoothTxThread(BluetoothSocket socket) throws IOException {
    this._outputStream = socket.getOutputStream();
    this._sendQueue = new LinkedBlockingQueue<>();
    this._running = new AtomicBoolean(true);
    start();
  }

  public boolean isRunning() {
    return _running.get();
  }

  @Override
  public void run() {
    try {
      while (_running.get() && !Thread.currentThread().isInterrupted()) {
        byte[] data = _sendQueue.poll();
        if (data != null) {
          _outputStream.write(data);
          _outputStream.flush();
        }
        ThreadUtil.sleep();
      }
    } catch (Exception e) {
      Log.d(TAG, "Run failed:", e);
    } finally {
      _running.set(false);
    }
    Log.d(TAG, "Bluetooth TX thread terminated");
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
    interrupt();
  }
}
