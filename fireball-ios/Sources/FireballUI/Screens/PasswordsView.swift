import SwiftUI
import FireballCore

public struct PasswordsView: View {
    public let vault: PasswordVault
    public let onDismiss: () -> Void

    @State private var credentials: [SavedCredential] = []
    @State private var searchText = ""
    @State private var showAddSheet = false

    @State private var newDomain = ""
    @State private var newUsername = ""
    @State private var newPassword = ""

    public init(vault: PasswordVault, onDismiss: @escaping () -> Void) {
        self.vault = vault
        self.onDismiss = onDismiss
    }

    public var filteredCredentials: [SavedCredential] {
        if searchText.isEmpty {
            return credentials
        }
        return credentials.filter {
            $0.domain.localizedCaseInsensitiveContains(searchText) ||
            $0.username.localizedCaseInsensitiveContains(searchText)
        }
    }

    public var body: some View {
        NavigationStack {
            List {
                if filteredCredentials.isEmpty {
                    VStack(spacing: 12) {
                        Image(systemName: "key.fill")
                            .font(.system(size: 36))
                            .foregroundColor(FireballTheme.mutedText)
                        Text("No Passwords Saved")
                            .font(.system(size: 16, weight: .semibold))
                            .foregroundColor(FireballTheme.primaryText)
                        Text("Credentials saved during login will be securely stored here with AES-256-GCM.")
                            .font(.system(size: 13))
                            .foregroundColor(FireballTheme.secondaryText)
                            .multilineTextAlignment(.center)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 40)
                    .listRowBackground(FireballTheme.cardSurface)
                } else {
                    ForEach(filteredCredentials) { cred in
                        VStack(alignment: .leading, spacing: 4) {
                            Text(cred.domain)
                                .font(.system(size: 15, weight: .semibold))
                                .foregroundColor(FireballTheme.primaryText)
                            Text(cred.username)
                                .font(.system(size: 13))
                                .foregroundColor(FireballTheme.secondaryText)
                        }
                        .padding(.vertical, 4)
                        .listRowBackground(FireballTheme.cardSurface)
                    }
                }
            }
            .searchable(text: $searchText, prompt: "Search credentials")
            .scrollContentBackground(.hidden)
            .background(FireballTheme.background)
            .navigationTitle("Password Vault")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Close", action: onDismiss)
                        .foregroundColor(FireballTheme.secondaryText)
                }
                ToolbarItem(placement: .primaryAction) {
                    Button(action: { showAddSheet = true }) {
                        Image(systemName: "plus")
                            .foregroundColor(FireballTheme.electricLime)
                    }
                }
            }
            .sheet(isPresented: $showAddSheet) {
                NavigationStack {
                    Form {
                        Section(header: Text("NEW CREDENTIAL")) {
                            TextField("Domain (e.g. github.com)", text: $newDomain)
                            TextField("Username / Email", text: $newUsername)
                            SecureField("Password", text: $newPassword)
                        }
                    }
                    .navigationTitle("Add Account")
                    .toolbar {
                        ToolbarItem(placement: .cancellationAction) {
                            Button("Cancel") { showAddSheet = false }
                        }
                        ToolbarItem(placement: .confirmationAction) {
                            Button("Save") {
                                Task {
                                    _ = try? await vault.saveCredential(
                                        domain: newDomain,
                                        username: newUsername,
                                        plainPassword: newPassword
                                    )
                                    credentials = await vault.getAllCredentials()
                                    newDomain = ""
                                    newUsername = ""
                                    newPassword = ""
                                    showAddSheet = false
                                }
                            }
                            .disabled(newDomain.isEmpty || newUsername.isEmpty || newPassword.isEmpty)
                        }
                    }
                }
            }
            .task {
                credentials = await vault.getAllCredentials()
            }
        }
    }
}
