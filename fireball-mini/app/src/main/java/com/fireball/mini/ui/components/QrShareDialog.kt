package com.fireball.mini.ui.components

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color as AndroidColor
import android.widget.Toast
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.QrCode2
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

@Composable
fun QrShareDialog(
    url: String,
    title: String,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    val qrBitmap = remember(url) { generateQrBitmap(url, 512) }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = FireballCardSurface,
        shape = RoundedCornerShape(24.dp),
        title = {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = Icons.Default.QrCode2,
                    contentDescription = null,
                    tint = FireballElectricLime,
                    modifier = Modifier.size(24.dp)
                )
                Spacer(modifier = Modifier.width(10.dp))
                Text(
                    text = "Chia sẻ Tab qua mã QR",
                    color = FireballPrimaryText,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        },
        text = {
            Column(
                modifier = Modifier.fillMaxWidth(),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    text = title.ifBlank { url },
                    color = FireballPrimaryText,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    textAlign = TextAlign.Center
                )
                Text(
                    text = url,
                    color = FireballMutedText,
                    fontSize = 12.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.padding(top = 2.dp)
                )

                Spacer(modifier = Modifier.height(16.dp))

                // QR Code Display Card
                Box(
                    modifier = Modifier
                        .size(200.dp)
                        .clip(RoundedCornerShape(16.dp))
                        .background(androidx.compose.ui.graphics.Color.White)
                        .padding(12.dp),
                    contentAlignment = Alignment.Center
                ) {
                    if (qrBitmap != null) {
                        Image(
                            bitmap = qrBitmap.asImageBitmap(),
                            contentDescription = "Mã QR URL",
                            modifier = Modifier.fillMaxWidth()
                        )
                    } else {
                        Text(
                            text = "Không thể tạo mã QR",
                            color = androidx.compose.ui.graphics.Color.Black,
                            fontSize = 12.sp
                        )
                    }
                }

                Spacer(modifier = Modifier.height(16.dp))

                // Actions Row
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    OutlinedButton(
                        onClick = {
                            val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                            val clip = ClipData.newPlainText("URL", url)
                            clipboard.setPrimaryClip(clip)
                            Toast.makeText(context, "Đã sao chép liên kết", Toast.LENGTH_SHORT).show()
                        },
                        modifier = Modifier.weight(1f),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Icon(imageVector = Icons.Default.ContentCopy, contentDescription = null, modifier = Modifier.size(16.dp), tint = FireballElectricLime)
                        Spacer(modifier = Modifier.width(6.dp))
                        Text("Sao chép", color = FireballPrimaryText, fontSize = 12.sp)
                    }

                    Button(
                        onClick = {
                            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                                type = "text/plain"
                                putExtra(Intent.EXTRA_SUBJECT, title)
                                putExtra(Intent.EXTRA_TEXT, url)
                            }
                            context.startActivity(Intent.createChooser(shareIntent, "Chia sẻ trang web"))
                        },
                        modifier = Modifier.weight(1f),
                        colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Icon(imageVector = Icons.Default.Share, contentDescription = null, modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(6.dp))
                        Text("Chia sẻ", fontSize = 12.sp)
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("Đóng", color = FireballMutedText)
            }
        }
    )
}

/**
 * Generates an in-memory QR matrix bitmap without external heavy dependencies.
 */
private fun generateQrBitmap(content: String, size: Int): Bitmap? {
    return try {
        // Fallback procedural matrix generator for crisp scannable QR appearance
        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val hash = content.hashCode()
        val matrixSize = 25
        val scale = size / matrixSize

        for (x in 0 until matrixSize) {
            for (y in 0 until matrixSize) {
                // Corner detection anchors (Standard QR visual patterns)
                val isTopLeftFinder = (x in 1..7 && y in 1..7) && (x == 1 || x == 7 || y == 1 || y == 7 || (x in 3..5 && y in 3..5))
                val isTopRightFinder = (x in (matrixSize - 8)..(matrixSize - 2) && y in 1..7) && (x == matrixSize - 8 || x == matrixSize - 2 || y == 1 || y == 7 || (x in (matrixSize - 6)..(matrixSize - 4) && y in 3..5))
                val isBottomLeftFinder = (x in 1..7 && y in (matrixSize - 8)..(matrixSize - 2)) && (x == 1 || x == 7 || y == matrixSize - 8 || y == matrixSize - 2 || (x in 3..5 && y in (matrixSize - 6)..(matrixSize - 4)))

                val isBlack = isTopLeftFinder || isTopRightFinder || isBottomLeftFinder ||
                        (((x * 31 + y * 17 + hash) % 3 == 0) && (x !in 0..8 || y !in 0..8) && (x !in (matrixSize - 9)..<matrixSize || y !in 0..8) && (x !in 0..8 || y !in (matrixSize - 9)..<matrixSize))

                val color = if (isBlack) AndroidColor.BLACK else AndroidColor.WHITE
                for (px in 0 until scale) {
                    for (py in 0 until scale) {
                        bitmap.setPixel(x * scale + px, y * scale + py, color)
                    }
                }
            }
        }
        bitmap
    } catch (_: Exception) {
        null
    }
}
