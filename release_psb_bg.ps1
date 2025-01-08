###################CONFIG##################
$user = "vanishingfork"
$repo = "psb_bg"
$token_filename = "token.env"
###################CONFIG##################

# Clean the solution
MSBuild psb_bg.sln /t:Clean
if ($LASTEXITCODE -ne 0) {
    Write-Host "Clean failed!" -ForegroundColor Red
    exit 1
}

# Build the binary
MSBuild psb_bg.sln
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# GitHub API configuration. Takes a PAT, user, and repo as input
$envPath = Join-Path $PSScriptRoot $token_filename
if (!(Test-Path $envPath)) {
    throw "token.env file not found!"
}
$token = (Get-Content $envPath).Split('=')[1].Trim()
$headers = @{
    "Authorization" = "token $token"
    "Accept" = "application/vnd.github.v3+json"
}
# Get most recent tag from repo
try {
    $latestRelease = Invoke-RestMethod -Uri "https://api.github.com/repos/${user}/${repo}/releases" `
        -Method Get -Headers $headers
    $latestTag = $latestRelease[0].tag_name
}
catch {
    Write-Host "Error occurred during API call:" -ForegroundColor Red
    Write-Host "Status Code: $($_.Exception.Response.StatusCode.value__)" -ForegroundColor Red
    Write-Host "Status Description: $($_.Exception.Response.StatusDescription)" -ForegroundColor Red
    Write-Host "Error Message: $($_.Exception.Message)" -ForegroundColor Red
    
    Write-Host "`nNo existing releases found, defaulting to v0.0.0" -ForegroundColor Yellow
    $latestTag = "v0.0.0"
}
# Get release details
$releaseTag = Read-Host "Enter release tag (latest tag: $latestTag)"
$releaseDescription = Read-Host "Enter release description"
MSBuild psb_bg.sln
# Create release
$releaseData = @{
    tag_name = $releaseTag
    name = $releaseTag
    body = $releaseDescription
    draft = $false
    prerelease = $false
} | ConvertTo-Json

try {
    $response = Invoke-RestMethod -Uri "https://api.github.com/repos/${user}/${repo}/releases" `
        -Method Post -Headers $headers -Body $releaseData
    
    Write-Host "Release created successfully!" -ForegroundColor Green
    
    # Get upload URL and remove template parameters
    $uploadUrl = $response.upload_url -replace "\{\?name,label\}",""
    
    # Upload all files from dist folder
    Get-ChildItem "x64/Release/*.exe" | ForEach-Object {
        Write-Host "Uploading $($_.Name)..." -ForegroundColor Yellow
        
        $uploadHeaders = @{
            "Authorization" = "token $token"
            "Content-Type" = "application/octet-stream"
        }
        
        $fileBytes = [System.IO.File]::ReadAllBytes($_.FullName)
        $uploadResponse = Invoke-RestMethod -Uri "$uploadUrl`?name=$($_.Name)" `
            -Method Post -Headers $uploadHeaders -Body $fileBytes
            
        Write-Host "Uploaded $($_.Name) successfully!" -ForegroundColor Green
    }
}
catch {
    Write-Host "Error: $_" -ForegroundColor Red
    exit 1
}