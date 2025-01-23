import javax.swing.*;
import java.awt.*;

public class Main {
    private static JFrame window;
    public static JPanel mainPanel;
    private static CardLayout cardLayout;

    public static void main(String[] args) {
        // Initialize JFrame
        window = new JFrame("Panel Drawing Example");
        window.setSize(800, 700);
        window.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        window.setLocationRelativeTo(null);

        // Initialize CardLayout and mainPanel
        cardLayout = new CardLayout();
        mainPanel = new JPanel(cardLayout);

        QueuePanel queuePanel = new QueuePanel();
        GamePanel gamePanel = new GamePanel();

        queuePanel.setStatusText("Waiting for an opponent...");

        // Add panels to CardLayout
        mainPanel.add(new MainMenuPanel(cardLayout, mainPanel, window), "MenuPanel");
        mainPanel.add(gamePanel, "GamePanel");
        mainPanel.add(queuePanel, "QueuePanel");

        // Add mainPanel to JFrame
        window.add(mainPanel);
        window.setVisible(true);
    }

    public static void switchPanel(String panelName) {
        cardLayout.show(mainPanel, panelName);
    }

    public static JPanel getPanelByType(Class<? extends JPanel> panelClass) {
        for (Component component : mainPanel.getComponents()) {
            if (panelClass.isInstance(component)) {
                return (JPanel) component;
            }
        }
        return null; // Return null if the panel is not found
    }
    
}
