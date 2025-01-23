import javax.swing.*;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;
import java.awt.*;
import java.io.IOException;

public class MainMenuPanel extends JPanel {
    private JTextField usernameField;
    private JTextField ipField;
    private JButton playButton;
    private JButton quitButton;
    private String usernamePlaceholder = "Enter your username...";
    private String ipPlaceholder = "Enter IP:port";
    private Image topLeftImage;

    private JPanel mainPanel;
    private CardLayout cardLayout;

    public MainMenuPanel(CardLayout cardLayout, JPanel mainPanel, JFrame window) {
        this.mainPanel = mainPanel;
        this.cardLayout = cardLayout;

        // Load the icon image
        ImageIcon topLeftIcon = new ImageIcon("img/icon.png"); // Update with your image path
        this.topLeftImage = topLeftIcon.getImage();

        // Set layout
        setLayout(new GridBagLayout());

        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(10, 20, 20, 20);
        gbc.fill = GridBagConstraints.HORIZONTAL;
        gbc.gridx = 0;
        gbc.anchor = GridBagConstraints.CENTER;

        // Add spacing
        gbc.gridy = 0;
        add(Box.createVerticalStrut(80), gbc);

        // Add title label
        JLabel nameLabel = createTitleLabel();
        gbc.gridy = 1;
        add(nameLabel, gbc);

        // Add username field
        usernameField = createTextField(usernamePlaceholder);
        gbc.gridy = 2;
        add(usernameField, gbc);

        // Add IP field
        ipField = createTextField(ipPlaceholder);
        gbc.gridy = 3;
        add(ipField, gbc);

        // Add play button
        playButton = createPlayButton(window);
        gbc.gridy = 4;
        add(playButton, gbc);

        // Add quit button
        quitButton = createQuitButton();
        gbc.gridy = 5;
        add(quitButton, gbc);

        // Add document listener to username field
        usernameField.getDocument().addDocumentListener(new DocumentListener() {
            public void changedUpdate(DocumentEvent e) { updatePlayButton(); }
            public void removeUpdate(DocumentEvent e) { updatePlayButton(); }
            public void insertUpdate(DocumentEvent e) { updatePlayButton(); }

            private void updatePlayButton() {
                if (!usernameField.getText().trim().isEmpty() && !usernameField.getText().equals(usernamePlaceholder)) {
                    playButton.setEnabled(true);
                    playButton.setBackground(new Color(100, 180, 100, 255));
                } else {
                    playButton.setEnabled(false);
                    playButton.setBackground(new Color(100, 180, 100, 100));
                }
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
        g2d.drawImage(topLeftImage, 10, 10, this);
    }

    private JLabel createTitleLabel() {
        JLabel label = new JLabel("Welcome to Prší!") {
            @Override
            protected void paintComponent(Graphics g) {
                Graphics2D g2d = (Graphics2D) g;
                g2d.setColor(new Color(0, 0, 0, 150));
                g2d.fillRoundRect(0, 0, getWidth(), getHeight(), 10, 10);
                g2d.setColor(Color.WHITE);
                super.paintComponent(g);
            }
        };
        label.setOpaque(false);
        label.setForeground(Color.WHITE);
        label.setHorizontalAlignment(SwingConstants.CENTER);
        label.setFont(new Font("SansSerif", Font.BOLD, 40));
        return label;
    }

    private JTextField createTextField(String placeholder) {
        JTextField textField = new JTextField(15);
        textField.setText(placeholder);
        textField.setFont(new Font("SansSerif", Font.PLAIN, 18));
        textField.setPreferredSize(new Dimension(350, 40));
        textField.setBackground(new Color(230, 230, 230));
        textField.setForeground(Color.GRAY);
        textField.setBorder(BorderFactory.createEmptyBorder(5, 10, 5, 10));

        textField.addFocusListener(new java.awt.event.FocusListener() {
            @Override
            public void focusGained(java.awt.event.FocusEvent e) {
                if (textField.getText().equals(placeholder)) {
                    textField.setText("");
                    textField.setForeground(Color.BLACK);
                }
            }

            @Override
            public void focusLost(java.awt.event.FocusEvent e) {
                if (textField.getText().isEmpty()) {
                    textField.setText(placeholder);
                    textField.setForeground(Color.GRAY);
                }
            }
        });

        return textField;
    }

    private JButton createPlayButton(JFrame window) {
        JButton button = new JButton("Play");
        button.setFont(new Font("SansSerif", Font.BOLD, 18));
        button.setPreferredSize(new Dimension(250, 50));
        button.setEnabled(false);
        button.setFocusPainted(false);
        button.setBackground(new Color(100, 180, 100, 100));
        button.setOpaque(true);
        button.setForeground(Color.WHITE);
        button.setBorder(BorderFactory.createCompoundBorder(
                BorderFactory.createLineBorder(new Color(80, 150, 80, 150), 2),
                BorderFactory.createEmptyBorder(5, 15, 5, 15)));
    
        button.addActionListener(e -> {
            String username = usernameField.getText().trim();
            String ipPortInput = ipField.getText().trim();
            String[] parts = ipPortInput.split(":");
    
            if (parts.length != 2) {
                JOptionPane.showMessageDialog(window, "Invalid format. Please enter in IP:Port format.", "Input Error", JOptionPane.ERROR_MESSAGE);
                return;
            }
    
            String serverIp = parts[0].trim();
            int serverPort;
            try {
                serverPort = Integer.parseInt(parts[1].trim());
            } catch (NumberFormatException ex) {
                JOptionPane.showMessageDialog(window, "Invalid Port number. Please enter a valid port.", "Input Error", JOptionPane.ERROR_MESSAGE);
                return;
            }
    
            // Connect to the server
            try {
                ConnectionManager connectionManager = ConnectionManager.getInstance(serverIp, serverPort, username);
                connectionManager.connect();
                System.out.println("Connected to server at " + serverIp + ":" + serverPort);
    
                // Retrieve panels and set ConnectionManager
                QueuePanel queuePanel = (QueuePanel) Main.getPanelByType(QueuePanel.class);
    
                if (queuePanel != null) {
                    queuePanel.setConnectionManager(connectionManager);
                } else {
                    System.err.println("QueuePanel not found!");
                }

                connectionManager.sendPlayerAction(null, "enter");
                // Switch to the Queue Panel
                Main.switchPanel("QueuePanel");

    
            } catch (IOException ioException) {
                JOptionPane.showMessageDialog(window, "Failed to connect to server.", "Connection Error", JOptionPane.ERROR_MESSAGE);
                ioException.printStackTrace();
            }
        });
    
        return button;
    }
    
    private JButton createQuitButton() {
        JButton button = new JButton("Quit");
        button.setFont(new Font("SansSerif", Font.BOLD, 18));
        button.setPreferredSize(new Dimension(250, 50));
        button.setFocusPainted(false);
        button.setBackground(new Color(180, 80, 80, 100));
        button.setOpaque(true);
        button.setForeground(Color.WHITE);
        button.setBorder(BorderFactory.createCompoundBorder(
                BorderFactory.createLineBorder(new Color(150, 80, 80, 150), 2),
                BorderFactory.createEmptyBorder(5, 15, 5, 15)));

        button.addActionListener(e -> System.exit(0));

        return button;
    }
}
