import javax.swing.*;
import java.awt.*;
import java.io.File;
import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class GamePanel extends BasePanel {
    // UI components
    private JPanel mainPanel;
    private JLayeredPane opponentHandPanel;
    private JPanel playAreaPanel;
    private JLayeredPane playerHandPanel;
    private JLabel turnIndicator;
    private JLabel drawPileLabel;
    private JLabel discardPileLabel;
    private JLabel selectedCardLabel;
    private JButton skipConfirmationButton;
    private JPanel suitSelectionPanel;

    // Game data
    private String topDiscardedCardName;
    private List<String> playerHand;
    private List<String> opponentHand;
    private int playerTurn;
    private boolean forceDraw = false;
    private boolean gameOver = false;

    // Constructor
    public GamePanel() {
        initializeBoard();
    }

    // ===========================
    // Initialization Methods
    // ===========================

    public void initializeBoard() {
        // Set up the base layout and background
        setLayout(new BorderLayout());
        setBackground(new Color(50, 50, 50));
    
        // Reinitialize player and opponent hands
        playerHand = new ArrayList<>();
        opponentHand = new ArrayList<>();
    
        // Reset game data
        topDiscardedCardName = "";
        playerTurn = -1;
        forceDraw = false;
        gameOver = false;
    
        // Reset and initialize all relevant UI components
        if (turnIndicator == null) {
            turnIndicator = new JLabel("", SwingConstants.CENTER);
            turnIndicator.setForeground(Color.WHITE);
            turnIndicator.setFont(new Font("SansSerif", Font.BOLD, 30));
        }
        initializeOpponentPanel();
        initializePlayArea();
        initializePlayerPanel();
    }

    private void initializeOpponentPanel() {
        if (opponentHandPanel != null) {
            opponentHandPanel.removeAll();
        } else {
            opponentHandPanel = new JLayeredPane();
            opponentHandPanel.setPreferredSize(new Dimension(600, 200));
        }
    
        JPanel opponentPanelWrapper = new JPanel(new BorderLayout());
        opponentPanelWrapper.setOpaque(false);
    
        // Turn Indicator placed above opponent's hand
        JPanel turnIndicatorPanel = new JPanel(new FlowLayout(FlowLayout.CENTER));
        turnIndicatorPanel.setOpaque(false);
        turnIndicatorPanel.add(turnIndicator);
    
        // Add components to wrapper
        opponentPanelWrapper.add(turnIndicatorPanel, BorderLayout.NORTH);
        opponentPanelWrapper.add(opponentHandPanel, BorderLayout.CENTER);
    
        add(opponentPanelWrapper, BorderLayout.NORTH);
    }
    
    private void initializePlayArea() {
        if (playAreaPanel != null) {
            playAreaPanel.removeAll();
        } else {
            playAreaPanel = new JPanel(null);
            playAreaPanel.setOpaque(false);
            playAreaPanel.setPreferredSize(new Dimension(600, 250));
        }
    
        // Suit selection panel
        if (suitSelectionPanel != null) {
            suitSelectionPanel.removeAll();
        } else {
            suitSelectionPanel = new JPanel(new GridLayout(1, 4, 10, 10));
            suitSelectionPanel.setBounds(200, 180, 200, 50);
            suitSelectionPanel.setOpaque(false);
            suitSelectionPanel.setVisible(false);
        }
        addSuitButtons(suitSelectionPanel);
        playAreaPanel.add(suitSelectionPanel);
    
        // Draw pile
        if (drawPileLabel == null) {
            drawPileLabel = createCardLabel("img/back.png", 100, 150);
            drawPileLabel.setBounds(150, 20, 100, 150);
            addCardListener(drawPileLabel, "drawPile");
        }
        playAreaPanel.add(drawPileLabel);
    
        // Discard pile
        if (discardPileLabel == null) {
            discardPileLabel = new JLabel();
            discardPileLabel.setBounds(350, 20, 100, 150);
        }
        playAreaPanel.add(discardPileLabel);
    
        // Skip confirmation button
        if (skipConfirmationButton == null) {
            skipConfirmationButton = new JButton("Confirm Skip");
            skipConfirmationButton.setBounds(550, 80, 150, 40);
            skipConfirmationButton.setFont(new Font("Arial", Font.BOLD, 16));
            skipConfirmationButton.addActionListener(e -> handleSkipConfirmation());
        }
        skipConfirmationButton.setVisible(false);
        playAreaPanel.add(skipConfirmationButton);
    
        add(playAreaPanel, BorderLayout.CENTER);
    }
    
    private void initializePlayerPanel() {
        if (playerHandPanel != null) {
            playerHandPanel.removeAll();
        } else {
            playerHandPanel = new JLayeredPane();
            playerHandPanel.setPreferredSize(new Dimension(600, 200));
        }
    
        JPanel playerPanelWrapper = new JPanel(new BorderLayout());
        playerPanelWrapper.setOpaque(false);
        playerPanelWrapper.setBorder(BorderFactory.createEmptyBorder(0, 0, 20, 0));
        playerPanelWrapper.add(playerHandPanel, BorderLayout.CENTER);
        add(playerPanelWrapper, BorderLayout.SOUTH);
    }
    

    // ===========================
    // Event Handling
    // ===========================

    protected void handleMessage(String message) {
        if (message.startsWith("KIVUPSPLAYER_RECONNECTED")) {
            handleOpponentReconnected();
        } else if (message.startsWith("KIVUPSCARD_PLAYED_INVALID")) {
            turnIndicator.setText("INVALID MOVE");
        } else if (message.startsWith("KIVUPSCARD_PLAYED_VALID")) {
            if (message.contains("LAST_CARD_PLAYED")) {
                gameOver = true;
            }
            handleCardPlayed(message);
        } else if (message.startsWith("KIVUPSCARD_PLAYED_UPDATE")) {
            handleOpponentCardUpdate(message);
        } else if (message.startsWith("KIVUPSDRAW_SUCCESS")) {
            handleCardDraw(message);
        } else if (message.startsWith("KIVUPSCARD_DRAWN_UPDATE")) {
            handleOpponentDraw();
        } else if (message.startsWith("KIVUPSTURN_SWITCH")) {
            handleTurnSwitch(message);
        } else if (message.startsWith("KIVUPSSUIT_UPDATE")) {
            handleSuitChange(message);
        } else if (message.startsWith("KIVUPSSKIP_PENDING")) {
            handleSkipPending();
        } else if (message.startsWith("KIVUPSFORCEDRAW_PENDING")) {
            forceDraw = true;
        } else if (message.startsWith("KIVUPSOPPONENT_DISCONNECTED")) {
            handleOpponentDisconnectDisplay();
        } else if (message.startsWith("KIVUPSGAME_OVER")) {
            handleGameOverDisplay(message);
        } else if (message.startsWith("KIVUPSSESSION_TERMINATED")) {
            handleGameTerminationDisplay();
        } else if (message.startsWith("KIVUPSHEARTBEAT")) {
            heartbeatResponseToServer();
        } else {
            ConnectionManager connectionManager = ConnectionManager.getInstance();
            connectionManager.disconnect();
            Main.switchPanel("MenuPanel");
        }
    }

    private void handleGameOverDisplay(String message) {
        boolean victory = message.contains("VICTORY");
        String resultMessage = victory ? "You won the game 🎉" : "You lost the game 😞";
        resultMessage = resultMessage + ". " + "Going back to queue soon.";
        turnIndicator.setText(resultMessage);
    
        // Delay the transition to the queue panel by 3 seconds
        int delay = 3000; // milliseconds
        javax.swing.Timer timer = new javax.swing.Timer(delay, e -> {
            QueuePanel queuePanel = (QueuePanel) Main.getPanelByType(QueuePanel.class);
            queuePanel.setConnectionManager(ConnectionManager.getInstance());
            ConnectionManager.getInstance().sendPlayerAction(null, "enter");
            Main.switchPanel("QueuePanel");
        });
    
        timer.setRepeats(false); // Ensure the timer only runs once
        timer.start();
    }      
    
    private void handleOpponentReconnected() {
        turnIndicator.setText("The opponent has reconnected!");
        
    }  

    private void handleSkipPending() {
        skipConfirmationButton.setVisible(true);
        turnIndicator.setText("Opponent played an Ace. Confirm Skip or Play a Card!");
        playAreaPanel.revalidate();
        playAreaPanel.repaint();
    }
    
    private void handleOpponentDisconnectDisplay() {
        turnIndicator.setText("The opponent has disconnected!");
        // DISABLE PLAYER INTERACTION WITH THE BOARD
    }

    private void handleGameTerminationDisplay() {
        turnIndicator.setText("Game was terminated, going back to the queue...");
    
        // Delay the transition to the queue panel by 3 seconds
        int delay = 3000; // milliseconds
        javax.swing.Timer timer = new javax.swing.Timer(delay, e -> {
            QueuePanel queuePanel = (QueuePanel) Main.getPanelByType(QueuePanel.class);
            queuePanel.setConnectionManager(ConnectionManager.getInstance());
            ConnectionManager.getInstance().sendPlayerAction(null, "enter");
            Main.switchPanel("QueuePanel");
        });
    
        timer.setRepeats(false); // Ensure the timer only runs once
        timer.start();
    }

    private void handleSuitChange(String message) {
        String suit = message.split("\\|")[1].toLowerCase();
        switch (suit) {
            case "heart":
                turnIndicator.setText("Suit changed to HEART ❤️");
                discardPileLabel.setBorder(BorderFactory.createLineBorder(Color.RED, 5));
                break;
            case "green":
                turnIndicator.setText("Suit changed to GREEN 🍀");
                discardPileLabel.setBorder(BorderFactory.createLineBorder(Color.GREEN, 5));
                break;
            case "acorn":
                turnIndicator.setText("Suit changed to ACORN 🌰");
                discardPileLabel.setBorder(BorderFactory.createLineBorder(Color.YELLOW, 5));
                break;
            case "ball":
                turnIndicator.setText("Suit changed to BALL ⚽");
                discardPileLabel.setBorder(BorderFactory.createLineBorder(Color.ORANGE, 5));
                break;
            default:
                turnIndicator.setText("Invalid suit change.");
                discardPileLabel.setBorder(BorderFactory.createEmptyBorder());
                break;
        }
        discardPileLabel.repaint();
    }
    

    private void handleSkipConfirmation() {
        sendSkipConfirmToServer();
        skipConfirmationButton.setVisible(false);
        playAreaPanel.revalidate();
        playAreaPanel.repaint();
    }    

    private void handleQueenPlay() {
        turnIndicator.setText("Choose a suit for the Queen!");
        suitSelectionPanel.setVisible(true);
    }
       

    private void handleCardPlayed(String message) {
        String playedCard = message.split("\\|")[1];
        playCard(playedCard, selectedCardLabel);
    }

    private void handleOpponentCardUpdate(String message) {
        String playedCard = message.split("\\|")[1];
        discardPileLabel.setBorder(BorderFactory.createEmptyBorder()); // Clear border
        discardPileLabel.setIcon(new ImageIcon(loadCardImage("img/" + playedCard + ".png")));
        discardPileLabel.repaint();
        opponentHand.remove(0);
        updateOpponentHand();
        topDiscardedCardName = playedCard;
    }

    private void handleCardDraw(String message) {
        String drawnCard = message.split("\\|")[1];
        playerHand.add(drawnCard);
        updatePlayerHand();
    }

    private void handleOpponentDraw() {
        forceDraw = false;
        opponentHand.add("");
        updateOpponentHand();
    }

    private void handleTurnSwitch(String message) {
        playerTurn = Integer.parseInt(message.split("\\|")[1]);
    
        if (playerTurn == 1) {
            turnIndicator.setText("Your turn!");
        } else {
            turnIndicator.setText("Opponent's turn...");
            skipConfirmationButton.setVisible(false); // Hide the skip button
        }
        turnIndicator.repaint();
    }        

    // ===========================
    // UI Updates
    // ===========================

    private void playCard(String cardName, JLabel cardLabel) {
        // Remove the card from the player's hand and update UI
        playerHand.remove(cardName);
        playerHandPanel.remove(cardLabel);
        playerHandPanel.revalidate();
        playerHandPanel.repaint();

        discardPileLabel.setBorder(BorderFactory.createEmptyBorder()); // Clear border
        discardPileLabel.repaint();

        // Update the discard pile UI
        discardPileLabel.setIcon(new ImageIcon(loadCardImage("img/" + cardName + ".png")));
        selectedCardLabel = null;
        topDiscardedCardName = cardName;
    
        if (gameOver) {
            return;
        }
        // Handle special action if a queen is played
        if (cardName.contains("queen")) {
            handleQueenPlay();
        }
    
        // Update player's hand visually
        updatePlayerHand();
    }

    private void updatePlayerHand() {
        updateHand(playerHandPanel, playerHand, true);
    }

    private void updateOpponentHand() {
        updateHand(opponentHandPanel, opponentHand, false);
    }

    private void updateHand(JLayeredPane handPanel, List<String> hand, boolean isPlayer) {
        handPanel.removeAll();
        int cardWidth = 100;
        int cardSpacing = 30;
        int startX = 0;

        for (int i = 0; i < hand.size(); i++) {
            String card = isPlayer ? "img/" + hand.get(i) + ".png" : "img/back.png";
            JLabel cardLabel = createCardLabel(card, cardWidth, 150);
            cardLabel.setBounds(startX + i * cardSpacing, 50, cardWidth, 150);
            if (isPlayer) addCardListener(cardLabel, hand.get(i));
            handPanel.add(cardLabel, i);
        }

        handPanel.revalidate();
        handPanel.repaint();
    }

    private void moveToQueuePanel() {
        // SwingUtilities.invokeLater(() -> {
        //     // Retrieve the existing QueuePanel from the CardLayout
        //     QueuePanel queuePanel = getQueuePanel();
        //     if (queuePanel != null) {
        //         // Trigger any specific actions if needed
        //         sendRequeueToServer();
        //     }
    
        //     // Switch to the QueuePanel
        //     cardLayout.show(mainPanel, "QueueScreen");
        //     mainPanel.revalidate();
        //     mainPanel.repaint();
        // });
    }
    
    private QueuePanel getQueuePanel() {
        for (Component comp : mainPanel.getComponents()) {
            if (comp instanceof QueuePanel) {
                return (QueuePanel) comp; // Return the first found QueuePanel
            }
        }
        return null; // Return null if no QueuePanel exists
    }
     
    // ===========================
    // Utility Methods
    // ===========================

    private JLabel createCardLabel(String imagePath, int width, int height) {
        JLabel cardLabel = new JLabel();
        try {
            BufferedImage cardImage = ImageIO.read(new File(imagePath));
            ImageIcon cardIcon = new ImageIcon(cardImage.getScaledInstance(width, height, Image.SCALE_SMOOTH));
            cardLabel.setIcon(cardIcon);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return cardLabel;
    }

    private Image loadCardImage(String imagePath) {
        try {
            BufferedImage cardImage = ImageIO.read(new File(imagePath));
            return cardImage.getScaledInstance(100, 150, Image.SCALE_SMOOTH);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    private void addCardListener(JLabel cardLabel, String cardName) {
        cardLabel.addMouseListener(new java.awt.event.MouseAdapter() {
            @Override
            public void mouseClicked(java.awt.event.MouseEvent e) {
                handleCardClick(cardLabel, cardName);
            }
        });
    }

    private void handleCardClick(JLabel cardLabel, String cardName) {
        if (playerTurn != 1) return; // Only allow moves when it's the player's turn
    
        if (cardName.equals("drawPile")) {
            if (skipConfirmationButton.isVisible()) {
                drawPileLabel.setToolTipText("You cannot draw cards until you confirm the skip.");
                return;
            } else if (forceDraw) {
                sendForceDrawConfirmToServer();
                forceDraw = false;
                return;
            }
            drawCardFromServer();
        } else if (selectedCardLabel == cardLabel) {
            sendCardPlayToServer(cardName, topDiscardedCardName);
        } else {
            if (selectedCardLabel != null) {
                selectedCardLabel.setLocation(selectedCardLabel.getX(), selectedCardLabel.getY() + 20);
            }
            selectedCardLabel = cardLabel;
            cardLabel.setLocation(cardLabel.getX(), cardLabel.getY() - 20);
        }
    }    

    private void addSuitButtons(JPanel suitSelectionPanel) {
        String[] suits = {"Green", "Heart", "Acorn", "Ball"};
        Color[] colors = {Color.GREEN, Color.RED, Color.YELLOW, Color.ORANGE};
    
        for (int i = 0; i < suits.length; i++) {
            JButton suitButton = new JButton();
            suitButton.setBackground(colors[i]);
            suitButton.setOpaque(true);
            suitButton.setBorderPainted(false);
            suitButton.setToolTipText("Choose " + suits[i]);
            int finalI = i;
            suitButton.addActionListener(e -> {
                suitSelectionPanel.setVisible(false);
                sendQueenColorSelectionToServer(suits[finalI].toLowerCase());
                turnIndicator.setText("You selected: " + suits[finalI]);
            });
            suitSelectionPanel.add(suitButton);
        }
    }
    
    
    // ===========================
    // Server Communication
    // ===========================

    private void sendCardPlayToServer(String cardName, String discardedCardName) {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(cardName, "play");
    }

    private void drawCardFromServer() {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(null, "draw");
    }

    private void sendQueenColorSelectionToServer(String selectedColor) {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(selectedColor, "colorChange");
    }

    public void heartbeatResponseToServer() {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(null, "heartbeat");
    }

    private void sendRequeueToServer() {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(null, "requeue");
    }

    private void sendSkipConfirmToServer() {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(null, "skip");
    }

    private void sendForceDrawConfirmToServer() {
        ConnectionManager connectionManager = ConnectionManager.getInstance();
        connectionManager.sendPlayerAction(null, "forceDraw");
    }

    // ===========================
    // Game State Parsing
    // ===========================

    public void parseGameState(String gameState) {
        playerHand.clear();
        opponentHand.clear();
    
        playerHand.addAll(parsePlayerHand(gameState));
        opponentHand.addAll(parseOpponentHand(gameState));
        playerTurn = parsePlayerTurn(gameState);
        topDiscardedCardName = parseTopDiscard(gameState);
        discardPileLabel.setIcon(new ImageIcon(loadCardImage("img/" + topDiscardedCardName + ".png")));
    
        // Check for SKIP_PENDING or FORCE_DRAW_PENDING flags
        if (gameState.contains("SKIP_PENDING")) {
            handleSkipPending(); // Enable skip confirmation button
        } else {
            skipConfirmationButton.setVisible(false);
        }
    
        if (gameState.contains("FORCE_DRAW_PENDING")) {
            forceDraw = true; // Set force draw state
        } else {
            forceDraw = false;
        }
    
        updatePlayerHand();
        updateOpponentHand();
        if (playerTurn == 1) {
            turnIndicator.setText("Your turn!");
        } else {
            turnIndicator.setText("Opponent's turn...");
            skipConfirmationButton.setVisible(false); // Hide the skip button
        }
        turnIndicator.repaint();
    }
    

    private List<String> parsePlayerHand(String gameState) {
        List<String> hand = new ArrayList<>();
        if (gameState.contains("P1:") || gameState.contains("P2:")) {
            int start = gameState.indexOf("P1:") != -1 ? gameState.indexOf("P1:") + 3 : gameState.indexOf("P2:") + 3;
            int end = gameState.indexOf("|", start);
            if (end == -1) end = gameState.length();
            String[] cards = gameState.substring(start, end).split(",");
            hand.addAll(Arrays.asList(cards));
        }
        return hand;
    }

    private List<String> parseOpponentHand(String gameState) {
        List<String> hand = new ArrayList<>();
        if (gameState.contains("O:")) {
            int start = gameState.indexOf("O:") + 2;
            int end = gameState.indexOf("|", start);
            if (end == -1) end = gameState.length();
            try {
                int opponentCardCount = Integer.parseInt(gameState.substring(start, end));
                for (int i = 0; i < opponentCardCount; i++) {
                    hand.add("");
                }
            } catch (NumberFormatException e) {
                System.out.println("Failed to parse opponent card count.");
            }
        }
        return hand;
    }

    private int parsePlayerTurn(String gameState) {
        if (gameState.contains("T:")) {
            int start = gameState.indexOf("T:") + 2;
            int end = gameState.indexOf("|", start);
            if (end == -1) end = gameState.length();
            try {
                return Integer.parseInt(gameState.substring(start, end));
            } catch (NumberFormatException e) {
                System.out.println("Failed to parse player turn information.");
            }
        }
        return 0;
    }

    private String parseTopDiscard(String gameState) {
        if (gameState.contains("D:")) {
            int start = gameState.indexOf("D:") + 2;
            int end = gameState.indexOf("|", start);
            if (end == -1) end = gameState.length();
            return gameState.substring(start, end);
        }
        return null;
    }
}

/*
 * TODO:
 * 1) Currently when a player wins the game and gets requeued, the updateHand is not working properly
 */