import java.io.*;
import java.net.Socket;
import java.util.Timer;
import java.util.TimerTask;

public class ConnectionManager {
    private static ConnectionManager instance;
    private Socket socket;
    private BufferedReader reader;
    private PrintWriter writer;
    private final String serverIp;
    private final int serverPort;
    private MessageHandler messageHandler;
    private Thread listenerThread;
    private volatile boolean running = true;
    private volatile boolean reconnecting = false;
    private volatile boolean connected = true;
    private volatile long lastMessageReceivedTime;
    private String username; // Store the username for reconnection

    // Private constructor
    private ConnectionManager(String serverIp, int serverPort, String username) {
        this.serverIp = serverIp;
        this.serverPort = serverPort;
        this.username = username;
        this.lastMessageReceivedTime = System.currentTimeMillis();
    }

    public static ConnectionManager getInstance(String serverIp, int serverPort, String username) {
        if (instance == null) {
            instance = new ConnectionManager(serverIp, serverPort, username);
        }
        return instance;
    }

    public static ConnectionManager getInstance() {
        if (instance == null) {
            throw new IllegalStateException("ConnectionManager not initialized. Call getInstance(serverIp, serverPort, username) first.");
        }
        return instance;
    }

    public synchronized boolean isConnected() {
        return connected;
    }

    private synchronized void setConnected(boolean status) {
        connected = status;
    }

    private synchronized boolean isReconnecting() {
        return reconnecting;
    }

    public void connect() throws IOException {
        if (socket == null || socket.isClosed()) {
            socket = new Socket(serverIp, serverPort);
            reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            writer = new PrintWriter(socket.getOutputStream(), true);
            System.out.println("Connected to the server at " + serverIp + ":" + serverPort);
            if (!connected) sendReconnectMessage();
            startListening();
            // startConnectionMonitor();
        }
    }

    private void sendReconnectMessage() {
        String reconnectMessage = String.format("KIVUPSreconnect%04d%s", username.length(), username);
        sendMessage(reconnectMessage);
        System.out.println("Sent reconnect message: " + reconnectMessage);
    }

    private void startListening() {
        running = true;
        listenerThread = new Thread(() -> {
            try {
                String message;
                while (running && (message = receiveMessage()) != null) {
                    System.out.println("Received: " + message);

                    if (!message.startsWith("KIVUPSheartB")) {
                        lastMessageReceivedTime = System.currentTimeMillis();
                    }

                    if (messageHandler != null) {
                        messageHandler.handleMessage(message.trim());
                    }
                }
            } catch (IOException e) {
                if (running) {
                    System.out.println("Connection lost: " + e.getMessage());
                    setConnected(false);
                    handleDisconnection();
                }
            } finally {
                System.out.println("Listener thread stopped.");
                Main.switchPanel("MenuPanel");
            }
        });
        listenerThread.setDaemon(true);
        listenerThread.start();
    }

    private void startConnectionMonitor() {
        new Timer(true).scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                long currentTime = System.currentTimeMillis();
                if (isConnected() && currentTime - lastMessageReceivedTime > 5000) {
                    System.out.println("No valid message received in the last 5 seconds. Assuming connection lost.");
                    setConnected(false);
                    handleDisconnection();
                }
            }
        }, 0, 1000);
    }

    private String receiveMessage() throws IOException {
        if (reader != null) {
            return reader.readLine();
        }
        return null;
    }

    private void handleDisconnection() {
        synchronized (this) {
            if (reconnecting) {
                return;
            }
            reconnecting = true;
        }

        System.out.println("Connection lost. Attempting to reconnect...");

        new Thread(() -> {
            while (isReconnecting()) {
                try {
                    disconnect();
                    Thread.sleep(5000); // Retry every 5 seconds
                    connect();
                    setConnected(true);
                    synchronized (this) {
                        reconnecting = false;
                    }
                    System.out.println("Reconnection successful.");
                } catch (IOException | InterruptedException e) {
                    System.out.println("Reconnection failed: " + e.getMessage());
                }
            }
        }).start();
    }

    public void sendMessage(String message) {
        if (!isConnected()) {
            System.out.println("Cannot send message. Connection is lost.");
            return;
        }

        if (writer != null) {
            writer.println(message);
            System.out.println("Sent: " + message);
        } else {
            System.out.println("Writer is not initialized. Call connect() first.");
        }
    }

    public void sendPlayerAction(String data, String action) {
        if (!isConnected()) {
            System.out.println("Cannot send message. Connection is lost.");
            return;
        }

        String actionMessage = generateActionMessage(data, username, action);
        if (!actionMessage.isEmpty()) {
            sendMessage(actionMessage);
        }
    }

    private String generateActionMessage(String data, String username, String action) {
        switch (action) {
            case "enter":
                return String.format("KIVUPSenterQ%04d%s", username.length(), username);
            case "play":
                return String.format("KIVUPSplayCa%04d%s%04d%s", username.length(), username, data.length(), data);
            case "draw":
                return String.format("KIVUPSdrawCa%04d%s", username.length(), username);
            case "colorChange":
                return String.format("KIVUPSsuitCh%04d%s%04d%s", username.length(), username, data.length(), data);
            case "heartbeat":
                return String.format("KIVUPSheartB%04d%s", username.length(), username);
            case "requeue":
                return String.format("KIVUPSrQueue%04d%s", username.length(), username);
            case "skip":
                return String.format("KIVUPSskipMv%04d%s", username.length(), username);
            case "forceDraw":
                return String.format("KIVUPSforceD%04d%s", username.length(), username);
            default:
                System.out.println("Unknown action: " + action);
                return "";
        }
    }

    public void disconnect() {
        running = false;
        if (listenerThread != null && listenerThread.isAlive()) {
            listenerThread.interrupt();
        }
        try {
            if (socket != null && !socket.isClosed()) socket.close();
            if (reader != null) reader.close();
            if (writer != null) writer.close();
        } catch (IOException e) {
            System.out.println("Error while disconnecting: " + e.getMessage());
        } finally {
            listenerThread = null;
            System.out.println("Disconnected from the server.");
        }
    }

    public synchronized void setMessageHandler(MessageHandler handler) {
        this.messageHandler = handler;
    }

    public interface MessageHandler {
        void handleMessage(String message);
    }
}


// WHEN RECONNECTION WAS SUCCESSFUL, THEN START SENDING THE MESSAGE AND DONT DISCONNECT ANYMORE