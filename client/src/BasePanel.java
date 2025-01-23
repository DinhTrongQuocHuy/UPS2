import javax.swing.*;
import java.awt.*;

public abstract class BasePanel extends JPanel {
    protected ConnectionManager connectionManager;

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        Graphics2D g2d = (Graphics2D) g;

        // Default gradient background
        setBackground(new Color(50, 50, 50));
    }

    // Set the ConnectionManager and register the message handler
    public void setConnectionManager(ConnectionManager connectionManager) {
        this.connectionManager = connectionManager;

        if (connectionManager != null) {
            connectionManager.setMessageHandler(this::handleMessage);
        }
    }

    // Default message handler (can be overridden by subclasses)
    protected void handleMessage(String message) {}
}
