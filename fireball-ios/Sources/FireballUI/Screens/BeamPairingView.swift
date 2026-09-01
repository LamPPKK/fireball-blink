import SwiftUI
import FireballCore

public struct BeamPairingView: View {
    public let onDismiss: () -> Void

    @State private var serverAddress = "192.168.1.50:18080"
    @State private var invitationToken = ""
    @State private var connectionState: BeamClientState = .disconnected
    @State private var confirmationWords: [String] = ["fireball", "orbital", "plasma", "shield", "quantum", "stellar"]

    public init(onDismiss: @escaping () -> Void) {
        self.onDismiss = onDismiss
    }

    public var body: some View {
        NavigationStack {
            List {
                // Header Info
                Section {
                    VStack(alignment: .center, spacing: 10) {
                        Image(systemName: "antenna.radiowaves.left.and.right")
                            .font(.system(size: 40))
                            .foregroundColor(FireballTheme.electricLime)

                        Text("Fireball Beam (Remote Engine)")
                            .font(.system(size: 18, weight: .bold))
                            .foregroundColor(FireballTheme.primaryText)

                        Text("Offload heavy desktop Chromium tabs to your PC and stream high-performance 60 FPS video & audio directly to your iPhone or iPad.")
                            .font(.system(size: 13))
                            .foregroundColor(FireballTheme.secondaryText)
                            .multilineTextAlignment(.center)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .listRowBackground(FireballTheme.cardSurface)
                }

                // Pairing Form
                Section(header: Text("HOST DETAILS").foregroundColor(FireballTheme.secondaryText)) {
                    TextField("Server Endpoint (host:port)", text: $serverAddress)
                    TextField("Single-use Invitation Token", text: $invitationToken)
                }
                .listRowBackground(FireballTheme.cardSurface)

                // 6-word Confirmation Phrase
                Section(header: Text("6-WORD CONFIRMATION CODE").foregroundColor(FireballTheme.secondaryText)) {
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())], spacing: 8) {
                        ForEach(confirmationWords, id: \.self) { word in
                            Text(word)
                                .font(.system(size: 12, weight: .bold))
                                .foregroundColor(FireballTheme.electricLime)
                                .padding(.vertical, 6)
                                .frame(maxWidth: .infinity)
                                .background(FireballTheme.raisedSurface)
                                .cornerRadius(6)
                        }
                    }
                    .padding(.vertical, 4)
                }
                .listRowBackground(FireballTheme.cardSurface)

                // Action
                Section {
                    Button(action: {
                        connectionState = .connected(serverName: "MacBook Pro Desktop Core")
                    }) {
                        HStack {
                            Spacer()
                            Text(connectionState == .disconnected ? "Pair & Stream" : "Connected (60 FPS)")
                                .font(.system(size: 15, weight: .bold))
                                .foregroundColor(FireballTheme.background)
                            Spacer()
                        }
                    }
                    .listRowBackground(FireballTheme.electricLime)
                }
            }
            .scrollContentBackground(.hidden)
            .background(FireballTheme.background)
            .navigationTitle("Beam Pairing")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close", action: onDismiss)
                        .foregroundColor(FireballTheme.secondaryText)
                }
            }
        }
    }
}
