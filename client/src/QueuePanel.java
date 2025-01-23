import javax.swing.*;
import java.awt.*;

public class QueuePanel extends BasePanel {
    private JLabel statusLabel;
    private JLabel loadingLabel;

    public QueuePanel() {
        setLayout(new BorderLayout());
        setBackground(new Color(50, 50, 50));

        JPanel centerPanel = new JPanel();
        centerPanel.setLayout(new BoxLayout(centerPanel, BoxLayout.Y_AXIS));
        centerPanel.setOpaque(false);

        // Loading label
        loadingLabel = new JLabel(new ImageIcon("img/loading.gif"), SwingConstants.CENTER);
        loadingLabel.setAlignmentX(Component.CENTER_ALIGNMENT);
        centerPanel.add(loadingLabel);

        // Status label
        statusLabel = new JLabel("", SwingConstants.CENTER);
        statusLabel.setFont(new Font("Arial", Font.BOLD, 18));
        statusLabel.setForeground(Color.WHITE);
        statusLabel.setAlignmentX(Component.CENTER_ALIGNMENT);
        centerPanel.add(Box.createVerticalStrut(20));
        centerPanel.add(statusLabel);

        add(centerPanel, BorderLayout.CENTER);
    }

    // Setter method to update the text of the status label
    public void setStatusText(String text) {
        if (statusLabel != null) {
            statusLabel.setText(text);
        }
    }

    // Handle incoming messages
    protected void handleMessage(String message) {
        if (message.startsWith("KIVUPSgameSt")) {
            new Timer(500, e -> {
                handleGameStartMessage(message);
                ((Timer) e.getSource()).stop(); // Stop the timer
            }).start();
            
        } else if (message.startsWith("KIVUPSHEARTBEAT")) {
            ConnectionManager.getInstance().sendPlayerAction(null, "heartbeat");
        } else {
            if (ConnectionManager.getInstance() != null) {
                ConnectionManager.getInstance().disconnect();
            }
            Main.switchPanel("MenuPanel");
        }
    }

    private void handleGameStartMessage(String message) {
        SwingUtilities.invokeLater(() -> {
            GamePanel gamePanel = (GamePanel) Main.getPanelByType(GamePanel.class);

            if (gamePanel != null) {
                gamePanel.setConnectionManager(ConnectionManager.getInstance());
                gamePanel.parseGameState(message);
                Main.switchPanel("GamePanel");
            } else {
                System.err.println("GamePanel not found in CardLayout!");
            }
        });
    }

    @Override
    protected void paintComponent(Graphics g) {
        super.paintComponent(g);
        Graphics2D g2d = (Graphics2D) g;
        GradientPaint gradient = new GradientPaint(0, 0, new Color(50, 50, 50), getWidth(), getHeight(), new Color(80, 80, 80));
        g2d.setPaint(gradient);
        g2d.fillRect(0, 0, getWidth(), getHeight());
    }
}
